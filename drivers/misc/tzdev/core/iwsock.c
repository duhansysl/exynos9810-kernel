/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/socket.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include "lib/circ_buf.h"
#include "lib/circ_buf_packet.h"

#include <tzdev/tzdev.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/cred.h"
#include "core/event.h"
#include "core/iwio.h"
#include "core/iwsock.h"
#include "core/kthread_pool.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/sysdep.h"
#include "core/wait.h"

#define MAX_NUM_SOCKETS			(BITS_PER_INT * 16)
#define IWD_SOCKET_EVENTS_BUF_NUM_PAGES						\
		(round_up(							\
			sizeof(struct iwd_sock_events_buf),			\
			PAGE_SIZE) / PAGE_SIZE)

#define IWD_NUM_SWD_CONNECT_REQUESTS	256

#define DEFAULT_MAX_MSG_SIZE		0
#define DEFAULT_OOB_BUFFER_SIZE		128

/* Socket's transmission buffer layout.
 *
 * [.....write.buffer.....][.....read.buffer.....][..oob.write.buffer..]
 * |<--------------------------N * page-size-------------------------->|
 *
 * DEFAULT_TRANSMIT_BUFFER_SIZE defines default size of write and read buffers
 * available for transmitting data calculated to fit one page.
 */
#define DEFAULT_TRANSMIT_BUFFER_SIZE						\
	round_down(								\
		(PAGE_SIZE - 							\
			3 * round_up(CIRC_BUF_META_SIZE, sizeof(int32_t)) -	\
			2 * sizeof(int) - DEFAULT_OOB_BUFFER_SIZE) / 2,		\
		sizeof(int32_t))

/* Macros used for connection of circ buffers inside of shared socket's buffer */
#define GET_SOCKET_SEND_BUF(iwd_buf)						\
	(struct circ_buf *)((char *)iwd_buf + sizeof(struct iwd_sock_buf_head))

#define GET_SOCKET_RECEIVE_BUF(iwd_buf, rcv_buf_size)				\
	(struct circ_buf *)((char *)iwd_buf + 					\
		sizeof(struct iwd_sock_buf_head) + 				\
		circ_buf_total_size(rcv_buf_size))

#define GET_SOCKET_OOB_BUF(iwd_buf, rcv_buf_size, snd_buf_size)			\
	(struct circ_buf *)((char *)iwd_buf + 					\
		sizeof(struct iwd_sock_buf_head) +				\
		circ_buf_total_size(rcv_buf_size) +				\
		circ_buf_total_size(snd_buf_size))

enum operation {
	CONNECT_FROM_NWD,
	NEW_NWD_LISTEN,
	ACCEPT_FROM_NWD
};

enum state {
	NOT_INITIALIZED,
	READY,
	RELEASED
};

struct iwd_sock_events_stream_buf {
	unsigned int size;
	atomic_t num_events;
	uint32_t events[round_up(MAX_NUM_SOCKETS, BITS_PER_INT) / BITS_PER_INT];
};

struct iwd_sock_accept_errors_buf {
	unsigned int size;
	atomic_t num_events;
	uint32_t events[round_up(IWD_NUM_SWD_CONNECT_REQUESTS, BITS_PER_INT) / BITS_PER_INT];
};

struct iwd_sock_events_buf {
	struct iwd_sock_events_stream_buf nwd_events;
	struct iwd_sock_events_stream_buf swd_events;
	struct iwd_sock_accept_errors_buf accept_err_events;
} __packed;

struct iwd_sock_connect_request {
	int32_t swd_id;
	uint32_t snd_buf_size;
	uint32_t rcv_buf_size;
	uint32_t oob_buf_size;
	uint32_t max_msg_size;
} __packed;

struct connect_ext_data {
	enum operation op;
	struct sock_desc *sd;
	const char *name;
	int swd_id;
};

static DEFINE_IDR(tzdev_sock_map);
static DEFINE_MUTEX(tzdev_sock_map_lock);

static struct iwd_sock_events_buf *sock_events;

static DEFINE_MUTEX(nwd_events_list_lock);

static DECLARE_WAIT_QUEUE_HEAD(tz_iwsock_wq);

static struct task_struct *notification_kthread;

static unsigned int tz_iwsock_state = NOT_INITIALIZED;

static void tz_iwsock_free_resources(struct sock_desc *sd)
{
	tz_iwio_free_iw_channel(sd->iwd_buf);

	kfree(sd);
}

static void tz_iwsock_get_sd(struct sock_desc *sd)
{
	atomic_inc(&sd->ref_count);
}

static struct sock_desc *tz_iwsock_get_sd_by_sid(unsigned int sid)
{
	struct sock_desc *sd;

	mutex_lock(&tzdev_sock_map_lock);
	sd = idr_find(&tzdev_sock_map, sid);
	if (sd)
		tz_iwsock_get_sd(sd);
	mutex_unlock(&tzdev_sock_map_lock);

	return sd;
}

void tz_iwsock_put_sd(struct sock_desc *sd)
{
	if (atomic_sub_return(1, &sd->ref_count) == 0)
		tz_iwsock_free_resources(sd);
}

static int tz_iwsock_publish_sd(struct sock_desc *sd)
{
	int sid;

	mutex_lock(&tzdev_sock_map_lock);

	sid = sysdep_idr_alloc_in_range(&tzdev_sock_map, sd, 0, MAX_NUM_SOCKETS);
	if (sid >= 0) {
		sd->id = sid;
		tz_iwsock_get_sd(sd);
	}

	mutex_unlock(&tzdev_sock_map_lock);

	return sid;
}

static void tz_iwsock_unpublish_sd(struct sock_desc *sd)
{
	mutex_lock(&tzdev_sock_map_lock);
	idr_remove(&tzdev_sock_map, sd->id);
	mutex_unlock(&tzdev_sock_map_lock);

	tz_iwsock_put_sd(sd);
}

unsigned long circ_buf_total_size(unsigned long size)
{
	return round_up(size + CIRC_BUF_META_SIZE, sizeof(u32));
}

static unsigned long connected_iwd_buf_total_size(struct sock_desc *sd)
{
	return round_up(sizeof(struct iwd_sock_buf_head) +
			circ_buf_total_size(sd->snd_buf_size) +
			circ_buf_total_size(sd->rcv_buf_size) +
			circ_buf_total_size(sd->oob_buf_size), PAGE_SIZE);
}

static unsigned long listen_iwd_buf_total_size(struct sock_desc *sd)
{
	return round_up(sizeof(struct iwd_sock_buf_head) +
			circ_buf_total_size(sd->snd_buf_size) +
			circ_buf_total_size(sd->rcv_buf_size), PAGE_SIZE);
}
static int tz_iwsock_pre_connect_callback(void *buf,
		unsigned long num_pages, void *ext_data)
{
	struct connect_ext_data *data = ext_data;
	struct sock_desc *sd = data->sd;
	struct iwd_sock_buf_head *iwd_buf = buf;
	struct circ_buf *rcv_buf, *snd_buf, *oob_buf;
	int32_t nwd_id;
	long ret;

	sd->iwd_buf = buf;

	snd_buf = GET_SOCKET_SEND_BUF(iwd_buf);
	rcv_buf = GET_SOCKET_RECEIVE_BUF(iwd_buf, sd->snd_buf_size);

	/* Write buffer */
	circ_buf_connect(&sd->write_buf, circ_buf_set(snd_buf), sd->snd_buf_size);

	/* Receive buffer */
	circ_buf_connect(&sd->read_buf, circ_buf_set(rcv_buf), sd->rcv_buf_size);

	iwd_buf->nwd_state = BUF_SK_CONNECTED;
	iwd_buf->swd_state = BUF_SK_NEW;

	nwd_id = sd->id;

	/* Place operation's ID into shared buffer */
	ret = circ_buf_write_packet(&sd->write_buf, (void *)&data->op,
			sizeof(data->op), CIRC_BUF_MODE_KERNEL);
	if (IS_ERR_VALUE(ret)) {
		log_error(tzdev_iwsock, "Failed to write operation id, error=%ld\n", ret);
		return ret;
	}

	/* Place socket's ID into shared buffer */
	ret = circ_buf_write_packet(&sd->write_buf, (void *)&nwd_id,
			sizeof(nwd_id), CIRC_BUF_MODE_KERNEL);
	if (IS_ERR_VALUE(ret)) {
		log_error(tzdev_iwsock, "Failed to write socket id, error=%ld\n", ret);
		return ret;
	}

	switch (data->op) {
	case CONNECT_FROM_NWD:
	case NEW_NWD_LISTEN:
		/* Place name of the socket to connect to or listening socket name
		 * in NWd into shared buffer. */
		ret = circ_buf_write_packet(&sd->write_buf, (char *)data->name,
				strlen(data->name) + 1, CIRC_BUF_MODE_KERNEL);
		if (IS_ERR_VALUE(ret)) {
			log_error(tzdev_iwsock, "Failed to write socket name, error=%ld\n", ret);
			return ret;
		}
		break;

	case ACCEPT_FROM_NWD:
		/* Place SWd ID into shared buffer to identify socket connecting
		 * from SWd */
		ret = circ_buf_write_packet(&sd->write_buf,
				(char *)&data->swd_id, sizeof(data->swd_id),
				CIRC_BUF_MODE_KERNEL);
		if (IS_ERR_VALUE(ret)) {
			log_error(tzdev_iwsock, "Failed to write socket swd_id, error=%ld\n", ret);
			return ret;
		}
		break;
	}

	/* Write buffers size */
	ret = circ_buf_write_packet(&sd->write_buf, (void *)&sd->snd_buf_size,
			sizeof(sd->snd_buf_size), CIRC_BUF_MODE_KERNEL);
	if (IS_ERR_VALUE(ret)) {
		log_error(tzdev_iwsock, "Failed to write snd_buf_size, error=%ld\n", ret);
		return ret;
	}

	ret = circ_buf_write_packet(&sd->write_buf, (void *)&sd->rcv_buf_size,
			sizeof(sd->rcv_buf_size), CIRC_BUF_MODE_KERNEL);
	if (IS_ERR_VALUE(ret)) {
		log_error(tzdev_iwsock, "Failed to write rcv_buf_size, error=%ld\n", ret);
		return ret;
	}

	switch (data->op) {
	case CONNECT_FROM_NWD:
	case ACCEPT_FROM_NWD:
		oob_buf = GET_SOCKET_OOB_BUF(iwd_buf,
				sd->snd_buf_size, sd->rcv_buf_size);

		/* Connect OOB buffer */
		circ_buf_connect(&sd->oob_buf,
				circ_buf_set(oob_buf), sd->oob_buf_size);

		ret = circ_buf_write_packet(&sd->write_buf,
				(void *)&sd->oob_buf_size,
				sizeof(sd->oob_buf_size), CIRC_BUF_MODE_KERNEL);
		if (IS_ERR_VALUE(ret)) {
			log_error(tzdev_iwsock, "Failed to write oob_buf_size, error=%ld\n", ret);
			return ret;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int tz_iwsock_check_ready(void)
{
	smp_rmb();

	return (tz_iwsock_state == READY);
}

static int tz_iwsock_check_init_done(void)
{
	smp_rmb();

	if (tz_iwsock_state == NOT_INITIALIZED)
		return -EAGAIN;
	else if (tz_iwsock_state == READY)
		return 0;
	else
		return -ENODEV;
}

static void tz_iwsock_notify_swd(int32_t sid)
{
	if (tz_event_add((struct iwd_events_buf *)&sock_events->nwd_events, sid))
		tz_kthread_pool_enter_swd();
}

static void tz_iwsock_notify_internally(int32_t sid)
{
	if (tz_event_add((struct iwd_events_buf *)&sock_events->swd_events, sid))
		wake_up(&tz_iwsock_wq);
}

struct sock_desc *tz_iwsock_socket(unsigned int is_kern, unsigned int is_interruptible)
{
	struct sock_desc *sd;
	int ret;

	/* Allocate sock_desc structure */
	sd = kmalloc(sizeof(struct sock_desc), GFP_KERNEL);
	if (!sd) {
		log_error(tzdev_iwsock, "Failed to allocate socket desc\n");
		return ERR_PTR(-ENOMEM);
	}

	memset(sd, 0, sizeof(struct sock_desc));

	atomic_set(&sd->ref_count, 1);
	init_waitqueue_head(&sd->wq);
	mutex_init(&sd->lock);

	sd->mm = ERR_PTR(-EINVAL);

	sd->cred.cmsghdr.cmsg_len = TZ_CMSG_LEN(sizeof(struct tz_cred));
	sd->cred.cmsghdr.cmsg_level = SOL_UNSPEC;
	sd->cred.cmsghdr.cmsg_type = TZ_SCM_CREDENTIALS;

	sd->snd_buf_size = DEFAULT_TRANSMIT_BUFFER_SIZE;
	sd->rcv_buf_size = DEFAULT_TRANSMIT_BUFFER_SIZE;
	sd->oob_buf_size = DEFAULT_OOB_BUFFER_SIZE;
	sd->max_msg_size = DEFAULT_MAX_MSG_SIZE;

	sd->is_kern = is_kern;
	sd->is_interruptible = is_interruptible;

	sd->state = TZ_SK_NEW;

	if ((ret = tz_iwsock_publish_sd(sd)) < 0) {
		log_error(tzdev_iwsock, "Failed to publish socket desc, error=%d\n", ret);
		tz_iwsock_put_sd(sd);
		sd = ERR_PTR(ret);
	}

	return sd;
}

static int tz_iwsock_copy_from(void *dst, void *src, int size, unsigned int is_kern)
{
	if (is_kern) {
		memcpy(dst, src, size);
		return 0;
	} else {
		return copy_from_user(dst, src, size);
	}
}

static int tz_iwsock_copy_to(void *dst, void *src, int size, unsigned int is_kern)
{
	if (is_kern) {
		memcpy(dst, src, size);
		return 0;
	} else {
		return copy_to_user(dst, src, size);
	}
}

static int tz_iwsock_getsockopt_socket(struct sock_desc *sd,
		int optname, void *optval, socklen_t *optlen)
{
	unsigned int size = sizeof(uint32_t);

	switch(optname) {
	case SO_SNDBUF:
		if (tz_iwsock_copy_to(optlen, &size, sizeof(size), sd->is_kern))
			return -EFAULT;

		if (tz_iwsock_copy_to(optval, &sd->snd_buf_size,
				sizeof(sd->snd_buf_size), sd->is_kern))
			return -EFAULT;

		return 0;

	case SO_RCVBUF:
		if (tz_iwsock_copy_to(optlen, &size, sizeof(size), sd->is_kern))
			return -EFAULT;

		if (tz_iwsock_copy_to(optval, &sd->rcv_buf_size,
				sizeof(sd->rcv_buf_size), sd->is_kern))
			return -EFAULT;

		return 0;

	default:
		return -ENOPROTOOPT;
	}
}

static int tz_iwsock_getsockopt_iwd(struct sock_desc *sd,
		int optname, void *optval, socklen_t *optlen)
{
	unsigned int size = sizeof(uint32_t);

	switch (optname) {
	case SO_IWD_MAX_MSG_SIZE:
		if (tz_iwsock_copy_to(optlen, &size, sizeof(size), sd->is_kern))
			return -EFAULT;

		if (tz_iwsock_copy_to(optval, &sd->max_msg_size,
				sizeof(sd->max_msg_size), sd->is_kern))
			return -EFAULT;

		return 0;
	default:
		return -ENOPROTOOPT;
	}
}

int tz_iwsock_getsockopt(struct sock_desc *sd, int level,
		int optname, void *optval, socklen_t *optlen)
{
	switch (level) {
	case SOL_SOCKET:
		return tz_iwsock_getsockopt_socket(sd, optname, optval, optlen);
	case SOL_IWD:
		return tz_iwsock_getsockopt_iwd(sd, optname, optval, optlen);
	default:
		return -EINVAL;
	}
}

static int tz_iwsock_setsockopt_socket(struct sock_desc *sd,
		int optname, void *optval, socklen_t optlen)
{
	unsigned int size;

	switch(optname) {
	case SO_SNDBUF:
		if (optlen != sizeof(unsigned int))
			return -EINVAL;
		if (tz_iwsock_copy_from(&size, optval, optlen, sd->is_kern))
			return -EFAULT;
		if (!size)
			return -EINVAL;

		sd->snd_buf_size = circ_buf_size_for_packet(size) +
				circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));

		return 0;

	case SO_RCVBUF:
		if (optlen != sizeof(unsigned int))
			return -EINVAL;
		if (tz_iwsock_copy_from(&size, optval, optlen, sd->is_kern))
			return -EFAULT;
		if (!size)
			return -EINVAL;

		sd->rcv_buf_size = circ_buf_size_for_packet(size) +
				circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));

		return 0;

	default:
		return -ENOPROTOOPT;
	}
}

static int tz_iwsock_setsockopt_iwd(struct sock_desc *sd,
		int optname, void *optval, socklen_t optlen)
{
	unsigned int size;

	switch (optname) {
	case SO_IWD_MAX_MSG_SIZE:
		if (optlen != sizeof(unsigned int))
			return -EINVAL;
		if (tz_iwsock_copy_from(&size, optval, optlen, sd->is_kern))
			return -EFAULT;

		if (size)
			sd->max_msg_size = circ_buf_size_for_packet(size) +
					circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));
		else
			sd->max_msg_size = 0;

		return 0;
	default:
		return -ENOPROTOOPT;
	}
}

int tz_iwsock_setsockopt(struct sock_desc *sd, int level,
		int optname, void *optval, socklen_t optlen)
{
	int ret;

	mutex_lock(&sd->lock);

	if (sd->state != TZ_SK_NEW) {
		ret = -EBUSY;
		goto unlock;
	}

	switch (level) {
	case SOL_SOCKET:
		ret = tz_iwsock_setsockopt_socket(sd, optname, optval, optlen);
		break;
	case SOL_IWD:
		ret = tz_iwsock_setsockopt_iwd(sd, optname, optval, optlen);
		break;
	default:
		ret = -EINVAL;
		break;
	}

unlock:
	mutex_unlock(&sd->lock);

	return ret;
}

int tz_iwsock_connect(struct sock_desc *sd, const char *name, int flags)
{
	struct connect_ext_data ext_data = {.op = CONNECT_FROM_NWD, .sd = sd, .name = name};
	void *buf;
	int ret = 0;

	if (unlikely(tz_iwsock_state != READY)) {
		log_error(tzdev_iwsock, "Failed to connect socket %s, subsystem is not ready\n", name);
		return -ENODEV;
	}

	smp_rmb();

	if (strnlen(name, TZ_FILE_NAME_LEN) == TZ_FILE_NAME_LEN) {
		log_error(tzdev_iwsock, "Failed to connect socket %s, too long name\n", name);
		return -ENAMETOOLONG;
	}

	mutex_lock(&sd->lock);

	switch (sd->state) {
	case TZ_SK_CONNECTED:
		ret = -EISCONN;
		goto wrong_state;
	case TZ_SK_CONNECT_IN_PROGRESS:
		ret = -EALREADY;
		goto wrong_state;
	case TZ_SK_NEW:
		sd->state = TZ_SK_CONNECT_IN_PROGRESS;
		break;
	case TZ_SK_LISTENING:
		ret = -EINVAL;
		goto wrong_state;
	case TZ_SK_RELEASED:
		BUG();
	}

	buf = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_SOCKET,
			connected_iwd_buf_total_size(sd) / PAGE_SIZE,
			tz_iwsock_pre_connect_callback,
			NULL, &ext_data);

	if (IS_ERR(buf)) {
		sd->state = TZ_SK_NEW;
		sd->iwd_buf = NULL;
		ret = PTR_ERR(buf);
		log_error(tzdev_iwsock, "Failed to connect socket %s, error=%d\n",
				name, ret);
		goto sock_connect_failed;
	}

	if (flags & MSG_DONTWAIT) {
		smp_rmb();
		/* Non-blocking mode */
		if (sd->iwd_buf->swd_state != BUF_SK_CONNECTED)
			ret = -EINPROGRESS;
		else
			sd->state = TZ_SK_CONNECTED;
	} else {
		mutex_unlock(&sd->lock);

		/* Blocking mode */
		ret = tz_iwsock_wait_connection(sd);
		return (ret == -ERESTARTSYS) ? -EINTR : ret;
	}

wrong_state:
	log_error(tzdev_iwsock, "Failed to connect socket %s, wrong state %d\n",
			name, sd->state);
sock_connect_failed:
	mutex_unlock(&sd->lock);

	return ret;
}

static int tz_iwsock_is_connection_done(struct sock_desc *sd)
{
	smp_rmb();

	return sd->state != TZ_SK_CONNECT_IN_PROGRESS ||
			sd->iwd_buf->swd_state != BUF_SK_NEW;
}

int tz_iwsock_wait_connection(struct sock_desc *sd)
{
	int ret = 0;

	if (unlikely(tz_iwsock_state != READY)) {
		log_error(tzdev_iwsock,
				"Connection wait is impossible, subsystem is not ready\n");
		return -ENODEV;
	}

	smp_rmb();

	mutex_lock(&sd->lock);

	switch (sd->state) {
	case TZ_SK_NEW:
	case TZ_SK_LISTENING:
		ret = -EINVAL;
		goto unlock_sd;

	case TZ_SK_CONNECT_IN_PROGRESS:
		break;

	case TZ_SK_CONNECTED:
		goto unlock_sd;

	case TZ_SK_RELEASED:
		BUG();
	}

	if (sd->is_kern && !sd->is_interruptible)
		wait_event_uninterruptible_freezable_nested(sd->wq,
				!tz_iwsock_check_ready() ||
				tz_iwsock_is_connection_done(sd));
	else
		ret = wait_event_interruptible_nested(sd->wq,
				!tz_iwsock_check_ready() ||
				tz_iwsock_is_connection_done(sd));

	if (ret)
		goto unlock_sd;

	if (!tz_iwsock_check_ready()) {
		ret = -ECONNREFUSED;
		log_error(tzdev_iwsock, "Socket is not ready, error=%d\n", ret);
		goto unlock_sd;
	}

	switch (sd->iwd_buf->swd_state) {
	case BUF_SK_CONNECTED:
		sd->state = TZ_SK_CONNECTED;
		ret = 0;
		break;
	case BUF_SK_CLOSED:
		ret = -ECONNREFUSED;
		tz_iwio_free_iw_channel(sd->iwd_buf);
		sd->state = TZ_SK_NEW;
		log_error(tzdev_iwsock, "Peer buffer was closed, error=%d\n", ret);
		break;
	default:
		break;
	}

unlock_sd:
	mutex_unlock(&sd->lock);

	return ret;
}

int tz_iwsock_listen(struct sock_desc *sd, const char *name)
{
	struct connect_ext_data ext_data = {.op = NEW_NWD_LISTEN, .sd = sd, .name = name};
	void *buf;
	unsigned int snd_buf_size = sd->snd_buf_size, rcv_buf_size = sd->rcv_buf_size;
	int ret = 0;

	if (sd->is_kern && !sd->is_interruptible)
		wait_event_uninterruptible_freezable_nested(tz_iwsock_wq,
				tz_iwsock_check_init_done() != -EAGAIN);
	else if ((ret = wait_event_interruptible_nested(
				tz_iwsock_wq, tz_iwsock_check_init_done() != -EAGAIN)))
		return ret;

	if (!tz_iwsock_check_ready()) {
		log_error(tzdev_iwsock, "Failed to bind socket as listening, subsystem is not ready\n");
		return -ENODEV;
	}

	if (strnlen(name, TZ_FILE_NAME_LEN) == TZ_FILE_NAME_LEN) {
		log_error(tzdev_iwsock, "Failed to bind socket as listening, too long name\n");
		return -ENAMETOOLONG;
	}

	mutex_lock(&sd->lock);

	if (sd->state != TZ_SK_NEW) {
		ret = -EBADF;
		log_error(tzdev_iwsock, "Failed to bind socket as listening, wrong state %d\n",
				sd->state);
		goto unlock;
	}

	strcpy(sd->listen_name, name);

	sd->snd_buf_size = DEFAULT_TRANSMIT_BUFFER_SIZE;
	sd->rcv_buf_size = DEFAULT_TRANSMIT_BUFFER_SIZE;

	buf = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_SOCKET,
			listen_iwd_buf_total_size(sd) / PAGE_SIZE,
			tz_iwsock_pre_connect_callback, NULL, &ext_data);

	if (IS_ERR(buf)) {
		sd->iwd_buf = NULL;

		sd->snd_buf_size = snd_buf_size;
		sd->rcv_buf_size = rcv_buf_size;

		ret = PTR_ERR(buf);
		log_error(tzdev_iwsock, "Failed to bind socket as listening, error=%d\n", ret);
	} else {
		sd->state = TZ_SK_LISTENING;
	}

unlock:
	mutex_unlock(&sd->lock);

	return ret;
}

struct sock_desc *tz_iwsock_accept(struct sock_desc *sd)
{
	struct connect_ext_data ext_data = {.op = ACCEPT_FROM_NWD};
	struct iwd_sock_connect_request conn_req;
	struct sock_desc *new_sd;
	void *iwd_buf;
	long ret;

	if (unlikely(tz_iwsock_state != READY)) {
		log_error(tzdev_iwsock, "Failed to accept socket, subsystem is not ready\n");
		return ERR_PTR(-ENODEV);
	}

	smp_rmb();

	mutex_lock(&sd->lock);
	if (sd->is_kern && !sd->is_interruptible)
		wait_event_uninterruptible_freezable_nested(sd->wq, !tz_iwsock_check_ready() ||
			sd->state != TZ_SK_LISTENING ||
			!circ_buf_is_empty(&sd->read_buf));
	else if ((ret = wait_event_interruptible_nested(sd->wq,
			!tz_iwsock_check_ready() ||
			sd->state != TZ_SK_LISTENING ||
			!circ_buf_is_empty(&sd->read_buf)))) {
		mutex_unlock(&sd->lock);
		return ERR_PTR(ret);
	}

	if (!tz_iwsock_check_ready() || sd->state != TZ_SK_LISTENING) {
		mutex_unlock(&sd->lock);
		log_error(tzdev_iwsock, "Failed to accept socket, not ready, state %d\n", sd->state);
		return ERR_PTR(-EINVAL);
	}

	ret = circ_buf_read(&sd->read_buf, (char *)&conn_req,
			sizeof(struct iwd_sock_connect_request), CIRC_BUF_MODE_KERNEL);
	mutex_unlock(&sd->lock);

	if (ret != sizeof(struct iwd_sock_connect_request)) {
		log_error(tzdev_iwsock, "Failed to accept socket, invalid request, error=%ld\n", ret);
		return ERR_PTR(ret);
	}

	new_sd = tz_iwsock_socket(sd->is_kern, sd->is_interruptible);
	if (IS_ERR(new_sd)) {
		ret = PTR_ERR(new_sd);
		log_error(tzdev_iwsock, "Failed to accept socket, failed to create new socket, error=%ld\n", ret);
		goto socket_allocation_failed;
	}

	new_sd->snd_buf_size = circ_buf_size_for_packet(conn_req.snd_buf_size) +
			circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));
	new_sd->rcv_buf_size = circ_buf_size_for_packet(conn_req.rcv_buf_size) +
			circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));
	new_sd->oob_buf_size = circ_buf_size_for_packet(conn_req.oob_buf_size) +
			circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));

	if (conn_req.max_msg_size)
		new_sd->max_msg_size = circ_buf_size_for_packet(conn_req.max_msg_size) +
				circ_buf_size_for_packet(sizeof(struct tz_cmsghdr_cred));

	ext_data.swd_id = conn_req.swd_id;

	ext_data.sd = new_sd;

	iwd_buf = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_SOCKET,
			connected_iwd_buf_total_size(new_sd) / PAGE_SIZE,
			tz_iwsock_pre_connect_callback,
			NULL, &ext_data);
	if (IS_ERR(iwd_buf)) {
		new_sd->iwd_buf = NULL;
		ret = PTR_ERR(iwd_buf);
		log_error(tzdev_iwsock, "Failed to accept socket, error=%ld\n", ret);
		goto iwio_connect_failed;
	}

	/* Mark new socket as connected */
	new_sd->state = TZ_SK_CONNECTED;

	return new_sd;

iwio_connect_failed:
	tz_iwsock_release(new_sd);

socket_allocation_failed:
	/* Write swd_id to unblock waiting threads in SWd */
	if (tz_event_add((struct iwd_events_buf *)&sock_events->accept_err_events,
			conn_req.swd_id))
		tz_kthread_pool_enter_swd();

	return ERR_PTR(ret);
}

void tz_iwsock_release(struct sock_desc *sd)
{
	unsigned int notify_swd = 0, notify_nwd = 0;
	struct iwd_sock_connect_request conn_req;

	mutex_lock(&sd->lock);

	switch (sd->state) {
	case TZ_SK_NEW:
		tz_iwsock_unpublish_sd(sd);
		break;

	case TZ_SK_LISTENING:
		/* Mark NWd state as closed to avoid new connect requests */
		sd->iwd_buf->nwd_state = BUF_SK_CLOSED;

		smp_mb();

		/* Read all requests and notify SWd IDs back to wake waiting threads */
		while (sd->iwd_buf->swd_state != BUF_SK_CLOSED &&
				circ_buf_read(&sd->read_buf, (char *)&conn_req,
				sizeof(conn_req), CIRC_BUF_MODE_KERNEL) > 0)
			tz_event_add((struct iwd_events_buf *)&sock_events->accept_err_events,
					conn_req.swd_id);

		/* Listen socket has connected IWd buffer, so proceed to check
		 * it's state. */
	case TZ_SK_CONNECT_IN_PROGRESS:
	case TZ_SK_CONNECTED:
		/* Mark NWd state as closed */
		sd->iwd_buf->nwd_state = BUF_SK_CLOSED;

		smp_mb();

		if (sd->iwd_buf->swd_state != BUF_SK_CLOSED)
			notify_swd = 1;
		else
			notify_nwd = 1;

		break;

	case TZ_SK_RELEASED:
		BUG();
	}

	sd->state = TZ_SK_RELEASED;

	mutex_unlock(&sd->lock);

	if (notify_swd)
		tz_iwsock_notify_swd(sd->id);
	else if (notify_nwd)
		tz_iwsock_notify_internally(sd->id);

	tz_iwsock_put_sd(sd);

	return;
}

static int __tz_iwsock_read(struct sock_desc *sd, struct circ_buf_desc *read_buf,
			struct tz_msghdr *msg, size_t len, int flags,
			unsigned int *need_notify)
{
	int mode = (sd->is_kern) ? CIRC_BUF_MODE_KERNEL : CIRC_BUF_MODE_USER;
	int swd_state;
	long ret;
	size_t nbytes;

	if (unlikely(tz_iwsock_state != READY)) {
		log_error(tzdev_iwsock, "Failed to read socket, subsystem is not ready\n");
		return -ENODEV;
	}

	mutex_lock(&sd->lock);

	switch (sd->state) {
	case TZ_SK_NEW:
	case TZ_SK_LISTENING:
	case TZ_SK_CONNECT_IN_PROGRESS:
		ret = -ENOTCONN;
		log_error(tzdev_iwsock, "Failed to read socket, not connected yet, error=%ld\n", ret);
		goto unlock;
	case TZ_SK_CONNECTED:
		break;
	case TZ_SK_RELEASED:
		BUG();
	}

	swd_state = sd->iwd_buf->swd_state;

	smp_rmb();

	if (msg->msg_control) {
		ret = circ_buf_read_packet_local(read_buf,
				(char *)msg->msg_control,
				sizeof(struct tz_cmsghdr_swd_cred),
				mode);
		if (IS_ERR_VALUE(ret)) {
			if (ret != -EAGAIN)
				log_error(tzdev_iwsock, "Failed to read socket, error=%ld\n", ret);
			goto recheck;
		} else if (ret != sizeof(struct tz_cmsghdr_swd_cred)) {
			ret = -EINVAL;
			log_error(tzdev_iwsock, "Failed to read socket, invalid packet size, error=%ld\n", ret);
			goto unlock;
		}
	} else {
		ret = circ_buf_drop_packet(read_buf);
		if (IS_ERR_VALUE(ret)) {
			if (ret != -EAGAIN)
				log_error(tzdev_iwsock, "Failed to read socket, drop packet failed, error=%ld\n", ret);
			goto recheck;
		}
	}

	ret = circ_buf_read_packet(read_buf, (char *)msg->msgbuf, len, mode);

	if (sd->max_msg_size) {
		nbytes = circ_buf_size_for_packet(sd->max_msg_size) * 2;

		if (circ_buf_bytes_free(read_buf) < nbytes)
			*need_notify = 1;
	} else {
		*need_notify = 1;
	}

recheck:
	if (ret == -EAGAIN && swd_state == BUF_SK_CLOSED)
		ret = 0;

unlock:
	mutex_unlock(&sd->lock);

	return ret;
}

ssize_t tz_iwsock_read_msg(struct sock_desc *sd,
		struct tz_msghdr *msg, int flags)
{
	int res = 0, ret = 0;
	unsigned int need_notify = 0;

	if (sd->is_kern && !sd->is_interruptible)
		wait_event_uninterruptible_freezable_nested(sd->wq,
				(ret = __tz_iwsock_read(sd, &sd->read_buf, msg,
				msg->msglen, flags, &need_notify)) != -EAGAIN);
	else
		res = wait_event_interruptible_nested(sd->wq,
				(ret = __tz_iwsock_read(sd, &sd->read_buf, msg,
				msg->msglen, flags, &need_notify)) != -EAGAIN);

	if (res)
		ret = res;

	if (ret > 0 && need_notify)
		tz_iwsock_notify_swd(sd->id);

	return ret;
}

ssize_t tz_iwsock_read(struct sock_desc *sd, void *buf, size_t count, int flags)
{
	struct tz_msghdr msghdr = {
		.msgbuf = buf,
		.msglen = count,
	};

	return tz_iwsock_read_msg(sd, &msghdr, flags);
}

static int tz_iwsock_format_cred_scm(struct sock_desc *sd)
{
	int ret;

	if (sd->mm == current->mm)
		return 0;

	ret = tz_format_cred(&sd->cred.cred);
	if (ret) {
		log_error(tzdev_uiwsock, "Failed to format socket credentials %d\n", ret);
		return ret;
	}

	sd->mm = current->mm;

	return ret;
}

static int __tz_iwsock_write(struct sock_desc *sd, struct circ_buf_desc *write_buf,
			void *scm_buf, size_t scm_len, void *data_buf, size_t data_len, int flags)
{
	long ret;
	int mode = (sd->is_kern) ? CIRC_BUF_MODE_KERNEL : CIRC_BUF_MODE_USER;

	if (unlikely(tz_iwsock_state != READY)) {
		log_error(tzdev_iwsock, "Failed to write socket, subsystem is not ready\n");
		return -ENODEV;
	}

	mutex_lock(&sd->lock);

	switch (sd->state) {
	case TZ_SK_NEW:
	case TZ_SK_LISTENING:
	case TZ_SK_CONNECT_IN_PROGRESS:
		ret = -ENOTCONN;
		log_error(tzdev_iwsock, "Failed to write socket, not connected yet, error=%ld\n", ret);
		goto unlock;
	case TZ_SK_CONNECTED:
		break;
	case TZ_SK_RELEASED:
		BUG();
	}

	smp_rmb();

	if (sd->iwd_buf->swd_state == BUF_SK_CLOSED) {
		ret = -ECONNRESET;
		log_error(tzdev_iwsock, "Failed to write socket, peer was closed, error=%ld\n", ret);
		goto unlock;
	}

	if (sd->max_msg_size && data_len > sd->max_msg_size) {
		ret = -EMSGSIZE;
		log_error(tzdev_iwsock, "Failed to write socket, invalid msg size, error=%ld\n", ret);
		goto unlock;
	}

	ret = circ_buf_write_packet_local(write_buf,
			(char *)scm_buf, scm_len, CIRC_BUF_MODE_KERNEL);
	if (IS_ERR_VALUE(ret))
		goto unlock;

	ret = circ_buf_write_packet(write_buf,
			(char *)data_buf, data_len, mode);
	if (IS_ERR_VALUE(ret))
		circ_buf_rollback_write(write_buf);

unlock:
	mutex_unlock(&sd->lock);

	return ret;
}

ssize_t tz_iwsock_write(struct sock_desc *sd, void *buf, size_t count, int flags)
{
	struct circ_buf_desc *write_buf;
	int ret, res = 0;

	write_buf = (flags & MSG_OOB) ? &sd->oob_buf : &sd->write_buf;

	if ((ret = tz_iwsock_format_cred_scm(sd)))
		return ret;

	if (sd->is_kern && !sd->is_interruptible)
		wait_event_uninterruptible_freezable_nested(sd->wq,
				(ret = __tz_iwsock_write(sd, write_buf, &sd->cred,
				sizeof(struct tz_cmsghdr_cred),
				buf, count, flags)) != -EAGAIN);
	else
		res = wait_event_interruptible_nested(sd->wq,
				(ret = __tz_iwsock_write(sd, write_buf, &sd->cred,
				sizeof(struct tz_cmsghdr_cred),
				buf, count, flags)) != -EAGAIN);

	if (res)
		ret = res;

	if (ret > 0)
		tz_iwsock_notify_swd(sd->id);

	return ret;
}

void tz_iwsock_wake_up_all(void)
{
	struct sock_desc *sd;
	int id;

	mutex_lock(&tzdev_sock_map_lock);
	idr_for_each_entry(&tzdev_sock_map, sd, id)
		wake_up(&sd->wq);
	mutex_unlock(&tzdev_sock_map_lock);
}

static void tz_iwsock_handle_event(unsigned int id)
{
	struct sock_desc *sd;

	sd = tz_iwsock_get_sd_by_sid(id);
	if (sd) {
		smp_rmb();
		if (sd->state == TZ_SK_RELEASED &&
				sd->iwd_buf->swd_state == BUF_SK_CLOSED)
			tz_iwsock_unpublish_sd(sd);
		else
				wake_up(&sd->wq);

		tz_iwsock_put_sd(sd);
	}
}

static int tz_iwsock_kthread(void *data)
{
	(void)data;

	while (!kthread_should_stop()) {
		wait_event_uninterruptible_freezable_nested(tz_iwsock_wq,
				tz_event_get_num_pending((struct iwd_events_buf *)&sock_events->swd_events) > 0 ||
				kthread_should_stop());

		if (kthread_should_stop())
			return 0;

		tz_event_process((struct iwd_events_buf *)&sock_events->swd_events,
				tz_iwsock_handle_event);
	}

	return 0;
}

static int tz_iwsock_events_pre_connect_callback(void *buf, unsigned long num_pages, void *ext_data)
{
	struct iwd_sock_events_buf *sock_buf = (struct iwd_sock_events_buf *)buf;

	tz_event_init((struct iwd_events_buf *)&sock_buf->nwd_events, MAX_NUM_SOCKETS);
	tz_event_init((struct iwd_events_buf *)&sock_buf->swd_events, MAX_NUM_SOCKETS);
	tz_event_init((struct iwd_events_buf *)&sock_buf->accept_err_events, IWD_NUM_SWD_CONNECT_REQUESTS);

	return 0;
}

static int tz_iwsock_post_smc_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (unlikely(tz_iwsock_state != READY))
		return NOTIFY_DONE;

	smp_rmb();

	if (tz_event_get_num_pending((struct iwd_events_buf *)&sock_events->swd_events) > 0)
		wake_up(&tz_iwsock_wq);

	return NOTIFY_DONE;
}

static struct notifier_block tz_iwsock_post_smc_notifier = {
	.notifier_call = tz_iwsock_post_smc_call,
};

int tz_iwsock_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;

	sock_events = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_SOCKET_EVENTS,
			IWD_SOCKET_EVENTS_BUF_NUM_PAGES,
			tz_iwsock_events_pre_connect_callback, NULL, NULL);
	if (IS_ERR(sock_events)) {
		ret = PTR_ERR(sock_events);
		sock_events = NULL;
		log_error(tzdev_iwsock, "Failed to initialize, IW sockets event buffer, error=%d\n", ret);
		return ret;
	}

	notification_kthread = kthread_run(tz_iwsock_kthread, NULL, "tz_iwsock");
	if (IS_ERR(notification_kthread)) {
		ret = PTR_ERR(notification_kthread);
		log_error(tzdev_iwsock, "Failed to run, IW sockets kernel thread, error=%d\n", ret);
		goto kthread_start_failed;
	}

	sched_setscheduler(notification_kthread, SCHED_FIFO, &param);

	ret = tzdev_atomic_notifier_register(TZDEV_POST_SMC_NOTIFIER, &tz_iwsock_post_smc_notifier);
	if (ret) {
		log_error(tzdev_iwsock, "Failed to register post smc notifier, error=%d\n", ret);
		goto post_smc_notifier_registration_failed;
	}

	tz_iwsock_state = READY;

	smp_wmb();

	wake_up(&tz_iwsock_wq);

	log_info(tzdev_iwsock, "IW sockets initialization done\n");

	return 0;

post_smc_notifier_registration_failed:
	kthread_stop(notification_kthread);
kthread_start_failed:
	tz_iwio_free_iw_channel(sock_events);
	notification_kthread = NULL;

	return ret;
}

void tz_iwsock_fini(void)
{
	if (likely(tz_iwsock_state == READY)) {
		smp_rmb();

		tz_iwsock_state = RELEASED;

		smp_wmb();

		tz_iwsock_wake_up_all();
		wake_up(&tz_iwsock_wq);
		tzdev_atomic_notifier_unregister(TZDEV_POST_SMC_NOTIFIER, &tz_iwsock_post_smc_notifier);
		kthread_stop(notification_kthread);
		notification_kthread = NULL;

		log_info(tzdev_iwsock, "IW sockets finalization done\n");
	}
}
