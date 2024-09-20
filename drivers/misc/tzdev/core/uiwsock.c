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

#include <linux/atomic.h>
#include <linux/anon_inodes.h>
#include <linux/module.h>
#include <linux/poll.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/iwsock.h"
#include "core/log.h"
#include "core/notifier.h"

#define IS_EINTR(x)				\
	(((x) == -EINTR)			\
	|| ((x) == -ERESTARTSYS)		\
	|| ((x) == -ERESTARTNOHAND)		\
	|| ((x) == -ERESTARTNOINTR)		\
	|| ((x) == -ERESTART_RESTARTBLOCK))

static atomic_t tz_uiwsock_init_done = ATOMIC_INIT(0);
static const struct file_operations tz_uiwsock_fops;

static int tz_uiwsock_open(struct inode *inode, struct file *filp)
{
	struct sock_desc *sd;

	sd = tz_iwsock_socket(0, TZ_INTERRUPTIBLE);
	if (IS_ERR(sd)) {
		log_error(tzdev_uiwsock, "Failed to create new socket, ret=%ld\n", PTR_ERR(sd));
		return PTR_ERR(sd);
	}

	filp->private_data = sd;

	return 0;
}

static long tz_uiwsock_connect(struct file *filp, unsigned long arg)
{
	long ret;
	struct tz_uiwsock_connection connection;
	struct sock_desc *sd = filp->private_data;
	struct tz_uiwsock_connection __user *argp = (struct tz_uiwsock_connection __user *)arg;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&connection, argp, sizeof(struct tz_uiwsock_connection))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n", argp, filp);
		return -EFAULT;
	}

	connection.name[TZ_UIWSOCK_MAX_NAME_LENGTH - 1] = 0;

	ret = tz_iwsock_connect(sd, connection.name, 0);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret < 0)
		log_error(tzdev_uiwsock, "Failed to connect to socket %s, filp=%pK, ret=%ld\n", connection.name, filp, ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_wait_connection(struct file *filp, unsigned long arg)
{
	long ret;
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;

	ret = tz_iwsock_wait_connection(sd);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret < 0)
		log_error(tzdev_uiwsock, "Failed to wait connection to socket, filp=%pK socket=%d ret=%ld\n",
				filp, sd->id, ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_listen(struct file *filp, unsigned long arg)
{
	long ret;
	struct tz_uiwsock_connection connection;
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct tz_uiwsock_connection __user *argp =
				(struct tz_uiwsock_connection __user *)arg;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&connection, argp, sizeof(struct tz_uiwsock_connection))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n",
									argp, filp);
		return -EFAULT;
	}

	connection.name[TZ_UIWSOCK_MAX_NAME_LENGTH - 1] = 0;

	ret = tz_iwsock_listen(sd, connection.name);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret)
		log_error(tzdev_uiwsock, "Failed to listen, ret=%ld\n", ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_accept(struct file *filp, unsigned long arg)
{
	struct sock_desc *sd = filp->private_data;
	struct sock_desc *new_sd;
	long ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	new_sd = tz_iwsock_accept(sd);
	if (IS_EINTR(PTR_ERR(new_sd))) {
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, PTR_ERR(new_sd));
		return PTR_ERR(new_sd);
	} else if (IS_ERR(new_sd)) {
		log_error(tzdev_uiwsock, "Failed to accept, ret=%ld\n", PTR_ERR(new_sd));
		return PTR_ERR(new_sd);
	}

	ret = anon_inode_getfd("[tz-uiwsock]", &tz_uiwsock_fops, new_sd, O_CLOEXEC);
	if (ret < 0) {
		log_error(tzdev_uiwsock, "Failed to get new anon inode fd, ret=%ld\n", ret);
		tz_iwsock_release(new_sd);
	}

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_send(struct file *filp, unsigned long arg)
{
	struct tz_uiwsock_data data;
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct tz_uiwsock_data __user *argp = (struct tz_uiwsock_data __user *)arg;
	long ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&data, argp, sizeof(struct tz_uiwsock_data))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n", argp, filp);
		return -EFAULT;
	}

	ret = tz_iwsock_write(sd, (void __user *)(uintptr_t)data.buffer,
			data.size, data.flags);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret < 0)
		log_error(tzdev_uiwsock, "Failed to write data filp=%pK, ret=%ld\n", filp, ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_recv_msg(struct file *filp, unsigned long arg)
{
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct tz_msghdr msg;
	struct tz_msghdr __user *argp = (struct tz_msghdr __user *)arg;
	long ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&msg, argp, sizeof(struct tz_msghdr))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n", argp, filp);
		return -EFAULT;
	}

	ret = tz_iwsock_read_msg(sd, &msg, msg.msg_flags);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret < 0)
		log_error(tzdev_uiwsock, "Failed to read data filp=%pK, ret=%ld\n", filp, ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

#ifdef CONFIG_COMPAT
static long tz_uiwsock_compat_recv_msg(struct file *filp, unsigned long arg)
{
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct compat_tz_msghdr compat_msg;
	struct tz_msghdr msg;
	struct compat_tz_msghdr __user *argp = (struct compat_tz_msghdr __user *)arg;
	long ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&compat_msg, argp, sizeof(struct compat_tz_msghdr))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n", argp, filp);
		return -EFAULT;
	}

	msg.msgbuf = (char *)(uintptr_t)compat_msg.msgbuf;
	msg.msglen = compat_msg.msglen;
	msg.msg_control = (char *)(uintptr_t)compat_msg.msg_control;
	msg.msg_controllen = compat_msg.msg_controllen;
	msg.msg_flags = compat_msg.msg_flags;

	ret = tz_iwsock_read_msg(sd, &msg, msg.msg_flags);
	if (IS_EINTR(ret))
		log_debug(tzdev_uiwsock, "Wait interrupted, filp=%pK, ret=%ld\n", filp, ret);
	else if (ret < 0)
		log_error(tzdev_uiwsock, "Failed to read data filp=%pK, ret=%ld\n", filp, ret);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}
#endif

static long tz_uiwsock_getsockopt(struct file *filp, unsigned long arg)
{
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct tz_uiwsock_sockopt __user *argp = (struct tz_uiwsock_sockopt __user *)arg;
	struct tz_uiwsock_sockopt sockopt;
	int ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&sockopt, argp, sizeof(struct tz_uiwsock_sockopt))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n",
				argp, filp);
		return -EFAULT;
	}

	ret = tz_iwsock_getsockopt(sd, sockopt.level, sockopt.optname,
			(void *)(uintptr_t)sockopt.optval,
			(socklen_t *)(uintptr_t)sockopt.optlen);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_setsockopt(struct file *filp, unsigned long arg)
{
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	struct tz_uiwsock_sockopt sockopt;
	struct tz_uiwsock_sockopt __user *argp = (struct tz_uiwsock_sockopt __user *)arg;
	int ret;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	if (copy_from_user(&sockopt, argp, sizeof(struct tz_uiwsock_sockopt))) {
		log_error(tzdev_uiwsock, "Invalid user space pointer %pK, filp=%pK\n",
				argp, filp);
		return -EFAULT;
	}

	ret = tz_iwsock_setsockopt(sd, sockopt.level, sockopt.optname,
			(void *)(uintptr_t)sockopt.optval, sockopt.optlen);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return ret;
}

static long tz_uiwsock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case TZIO_UIWSOCK_CONNECT:
		ret = tz_uiwsock_connect(filp, arg);
		break;
	case TZIO_UIWSOCK_WAIT_CONNECTION:
		ret = tz_uiwsock_wait_connection(filp, arg);
		break;
	case TZIO_UIWSOCK_SEND:
		ret = tz_uiwsock_send(filp, arg);
		break;
	case TZIO_UIWSOCK_RECV_MSG:
		ret = tz_uiwsock_recv_msg(filp, arg);
		break;
	case TZIO_UIWSOCK_LISTEN:
		ret = tz_uiwsock_listen(filp, arg);
		break;
	case TZIO_UIWSOCK_ACCEPT:
		ret = tz_uiwsock_accept(filp, arg);
		break;
	case TZIO_UIWSOCK_GETSOCKOPT:
		ret = tz_uiwsock_getsockopt(filp, arg);
		break;
	case TZIO_UIWSOCK_SETSOCKOPT:
		ret = tz_uiwsock_setsockopt(filp, arg);
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	if (IS_EINTR(ret))
		ret = -ERESTARTNOINTR;

	return ret;
}

#ifdef CONFIG_COMPAT
static long tz_uiwsock_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case TZIO_UIWSOCK_RECV_MSG:
		ret = tz_uiwsock_compat_recv_msg(filp, arg);
		break;
	default:
		ret = tz_uiwsock_ioctl(filp, cmd, arg);
		break;
	}

	if (IS_EINTR(ret))
		ret = -ERESTARTNOINTR;

	return ret;
}
#endif

static int tz_uiwsock_release(struct inode *inode, struct file *filp)
{
	struct sock_desc *sd = (struct sock_desc *)filp->private_data;
	(void)inode;

	log_debug(tzdev_uiwsock, "Enter, filp=%pK\n", filp);

	tz_iwsock_release(sd);

	log_debug(tzdev_uiwsock, "Exit, filp=%pK\n", filp);

	return 0;
}

static unsigned int tz_uiwsock_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct sock_desc *sd = filp->private_data;

	poll_wait(filp, &sd->wq, wait);

	if (sd->state != TZ_SK_CONNECTED && sd->state != TZ_SK_LISTENING)
		return 0;

	switch (sd->iwd_buf->nwd_state) {
	case BUF_SK_NEW:
		break;
	case BUF_SK_CONNECTED:
		if (!circ_buf_is_empty(&sd->read_buf))
			mask |= POLLIN | POLLRDNORM;

		switch(sd->iwd_buf->swd_state) {
		case BUF_SK_NEW:
			break;
		case BUF_SK_CLOSED:
			mask |= POLLHUP;
			break;
		case BUF_SK_CONNECTED:
			if (!circ_buf_is_full(&sd->write_buf))
				mask |= POLLOUT | POLLWRNORM;

			if (sd->state != TZ_SK_LISTENING) {
				if (!circ_buf_is_full(&sd->oob_buf))
					mask |= POLLOUT | POLLWRBAND;
			}

			break;
		}
		break;

	case BUF_SK_CLOSED:
		BUG();
	}

	return mask;
}

static const struct file_operations tz_uiwsock_fops = {
	.owner = THIS_MODULE,
	.open = tz_uiwsock_open,
	.poll = tz_uiwsock_poll,
	.unlocked_ioctl = tz_uiwsock_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_uiwsock_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.release = tz_uiwsock_release,
};

static struct tz_cdev tz_uiwsock_cdev = {
	.name = "tziwsock",
	.fops = &tz_uiwsock_fops,
	.owner = THIS_MODULE,
};

static int tz_uiwsock_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	int rc;

	(void)cb;
	(void)code;
	(void)unused;

	rc = tz_cdev_register(&tz_uiwsock_cdev);
	if (rc) {
		log_error(tzdev_uiwsock, "Failed to create boost device, error=%d\n", rc);
		return NOTIFY_DONE;
	}
	atomic_set(&tz_uiwsock_init_done, 1);

	log_info(tzdev_uiwsock, "IW sockets user interface initialization done.\n");

	return NOTIFY_DONE;
}

static int tz_uiwsock_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (!atomic_cmpxchg(&tz_uiwsock_init_done, 1, 0)) {
		log_info(tzdev_uiwsock, "IW sockets user interface not initialized.\n");
		return NOTIFY_DONE;
	}

	tz_cdev_unregister(&tz_uiwsock_cdev);

	log_info(tzdev_uiwsock, "IW sockets user interface finalization done.\n");

	return NOTIFY_DONE;
}

static struct notifier_block tz_uiwsock_init_notifier = {
	.notifier_call = tz_uiwsock_init_call,
};

static struct notifier_block tz_uiwsock_fini_notifier = {
	.notifier_call = tz_uiwsock_fini_call,
};

static int __init tz_uiwsock_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_uiwsock_init_notifier);
	if (rc) {
		log_error(tzdev_uiwsock, "Failed to register init notifier, error=%d\n", rc);
		return rc;
	}
	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &tz_uiwsock_fini_notifier);
	if (rc) {
		tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &tz_uiwsock_init_notifier);
		log_error(tzdev_uiwsock, "Failed to register fini notifier, error=%d\n", rc);
		return rc;
	}
	log_info(tzdev_uiwsock, "IW sockets callbacks registration done\n");

	return 0;
}

early_initcall(tz_uiwsock_init);
