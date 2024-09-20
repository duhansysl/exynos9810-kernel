/*
 * Copyright (C) 2012-2019 Samsung Electronics, Inc.
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

#include <linux/anon_inodes.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "tz_common.h"
#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/iwio.h"
#include "core/iwsock.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/sysdep.h"
#include "lib/circ_buf.h"
#include "lib/circ_buf_packet.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aleksandr Aleksandrov");
MODULE_DESCRIPTION("Trustzone file system driver");

#define LOG_KTHREAD "TZ_FSDEV_TH"
#define NWFS_CMD_SOCK "socket://nwfs_iwd"
#define SESSION_BUF_PAGES 5

#define TZ_FSDEV_IOC_MAGIC		'f'
#define TZ_FSDEV_INIT			_IO(TZ_FSDEV_IOC_MAGIC, 0)
#define TZ_FSDEV_ACCEPT			_IO(TZ_FSDEV_IOC_MAGIC, 1)
#define TZ_FSDEV_GET_CMD		_IOW(TZ_FSDEV_IOC_MAGIC, 2, struct tz_fsdev_packet)
#define TZ_FSDEV_REPLY			_IOR(TZ_FSDEV_IOC_MAGIC, 3, struct tz_fsdev_reply)
#define TZ_FSDEV_CREATE_SESSION_BUF	_IOR(TZ_FSDEV_IOC_MAGIC, 4, uint32_t)
#define TZ_FSDEV_FREE_SESSION_BUF	_IO(TZ_FSDEV_IOC_MAGIC, 5)
#define TZ_FSDEV_GET_WP_CMD		_IOW(TZ_FSDEV_IOC_MAGIC, 6, struct tz_fsdev_packet)
#define TZ_FSDEV_REPLY_WP_CMD		_IOR(TZ_FSDEV_IOC_MAGIC, 7, struct tz_fsdev_wp_reply)
#ifdef CONFIG_COMPAT
#define COMPAT_TZ_FSDEV_REPLY_WP_CMD	_IOR(TZ_FSDEV_IOC_MAGIC, 7, struct compat_tz_fsdev_wp_reply)
#endif

enum {
	NWFS_CMD_OPEN_FILE,
	NWFS_CMD_CLOSE_FILE,
	NWFS_CMD_READ_FILE,
	NWFS_CMD_READ_FILE_CONTINUE,
	NWFS_CMD_WRITE_FILE,
	NWFS_CMD_MKDIR,
	NWFS_CMD_RMDIR,
	NWFS_CMD_CLOSEDIR,
	NWFS_CMD_READDIR,
	NWFS_CMD_LSEEK_FILE,
	NWFS_CMD_UNLINK_FILE,
	NWFS_CMD_FSTAT_FILE,
	NWFS_CMD_TRUNCATE_FILE,
	NWFS_CMD_RENAME_FILE,
	NWFS_CMD_ALLOC_BUF,
	NWFS_CMD_FREE_BUF,
	NWFS_CMD_CNT,
};

#define BUF_CONTAINS_PFN	(1 << 1)

struct tz_fsdev_reply {
	uint32_t session_id;
	pid_t pid;
} __packed;

struct tz_fsdev_child_exit {
	pid_t pid;
	int32_t status;
} __packed;

/* fsdev <--> SK NWFS driver */
struct tz_fsdev_packet {
	uint32_t cmd;
	uint32_t ret;
	uint32_t packet_size;
	uint32_t flags;
	char buf[];
} __packed;

struct tz_fsdev_data {
	struct tz_uuid uuid;
	struct tz_fsdev_packet packet;
} __packed;

struct tz_fsdev_wp_reply {
	uint32_t ret;
	uint32_t size;
	uint32_t flags;
	void *buf;
} __packed;

#ifdef CONFIG_COMPAT
struct compat_tz_fsdev_wp_reply {
	uint32_t ret;
	uint32_t size;
	uint32_t flags;
	uint32_t buf;
} __packed;
#endif /* CONFIG_COMPAT */

struct tz_fsdev_ubuf_pin_info {
	struct page **pages;
	unsigned long nr_pages;
	struct mm_struct *mm;
	sk_pfn_t *pfns;
};

struct tz_fsdev_accept_data {
	uint32_t fd;
	uint32_t buf_size;
};

struct tz_fsdev_session_ctx {
	struct tz_fsdev_packet *session_buf;
	unsigned int session_buf_size;
	struct tz_fsdev_ubuf_pin_info pin_info;
	struct sock_desc *sd;
};

static struct tz_fsdev_session_ctx *nwfs_daemon_ctx;
struct sock_desc *fsdev_sock;

static int tz_fsdev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct tz_fsdev_session_ctx *create_session_ctx(void)
{
	struct tz_fsdev_session_ctx *session_ctx;

	session_ctx = kzalloc(sizeof(struct tz_fsdev_session_ctx), GFP_KERNEL);
	if (!session_ctx)
		return NULL;

	return session_ctx;
}

static void tz_fsdev_free_pfns(struct tz_fsdev_ubuf_pin_info *pin_info)
{
	if (pin_info->pfns) {
		tzdev_put_user_pages(pin_info->pages, pin_info->nr_pages);
		tzdev_decrease_pinned_vm(pin_info->mm, pin_info->nr_pages);
		mmput(pin_info->mm);
		kfree(pin_info->pfns);
		kfree(pin_info->pages);
	}
}

static void release_session_ctx(struct tz_fsdev_session_ctx *session_ctx)
{
	if (!session_ctx)
		return;
	tz_fsdev_free_pfns(&session_ctx->pin_info);
	tz_iwsock_release(session_ctx->sd);
	kfree(session_ctx->session_buf);
	kfree(session_ctx);
}

static int tz_fsdev_get_pfns_and_pin(void *buf, unsigned int size,
		struct tz_fsdev_ubuf_pin_info *pin_info)
{
	int ret;
	struct mm_struct *mm;
	struct task_struct *task;
	struct page **pages;
	sk_pfn_t *pfns;
	unsigned long nr_pages;
	unsigned int i;
	unsigned long start, end;

	if (!size)
		return -EINVAL;
	start = (unsigned long)buf >> PAGE_SHIFT;
	end = ((unsigned long)buf + size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	nr_pages = end - start;

	task = current;
	mm = get_task_mm(task);
	if (!mm)
		return -ESRCH;

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(pages)) {
		ret = -ENOMEM;
		goto out_mm;
	}

	pfns = kmalloc(nr_pages * sizeof(sk_pfn_t), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(pfns)) {
		ret = -ENOMEM;
		goto out_pages;
	}

	/*
	 * Holding 'mm->mmap_sem' is required to synchronize users who try to register same pages simultaneously.
	 * Migration is impossible without synchronization due to page refcount holding by both users.
	 */
	down_write(&mm->mmap_sem);
	ret = tzdev_get_user_pages(task, mm, (unsigned long __user)buf,
			nr_pages, 1, 0, pages, NULL);
	if (ret) {
		log_error(tzdev_nwfs, "Failed to pin user pages (%d)\n", ret);
		goto out_pfns;
	}

#if defined(CONFIG_TZDEV_PAGE_MIGRATION)
		/*
		 * In case of enabled migration it is possible that userspace pages
		 * will be migrated from current physical page to some other
		 * To avoid fails of CMA migrations we have to move pages to other
		 * region which can not be inside any CMA region. This is done by
		 * allocations with GFP_KERNEL flag to point UNMOVABLE memblock
		 * to be used for such allocations.
		 */
		ret = tzdev_migrate_pages(task, mm, (unsigned long __user)buf, nr_pages,
				1, 0, pages);
		if (ret < 0) {
			log_error(tzdev_nwfs, "Failed to migrate CMA pages (%d)\n", ret);
			goto out_pin;
		}
#endif /* CONFIG_TZDEV_PAGE_MIGRATION */
	up_write(&mm->mmap_sem);

	for (i = 0; i < nr_pages; i++)
		pfns[i] = page_to_pfn(pages[i]);

	pin_info->pages = pages;
	pin_info->nr_pages = nr_pages;
	pin_info->mm = mm;
	pin_info->pfns = pfns;

	return 0;

#if defined(CONFIG_TZDEV_PAGE_MIGRATION)
out_pin:
	tzdev_put_user_pages(pages, nr_pages);
	tzdev_decrease_pinned_vm(mm, nr_pages);
#endif /* CONFIG_TZDEV_PAGE_MIGRATION */

out_pfns:
	up_write(&mm->mmap_sem);
	kfree(pfns);
out_pages:
	kfree(pages);
out_mm:
	mmput(mm);

	pin_info->pages = NULL;
	pin_info->nr_pages = 0;
	pin_info->mm = NULL;
	pin_info->pfns = NULL;

	return ret;
}

static int tz_fsdev_short_reply(struct tz_fsdev_wp_reply *wp_reply,
		struct tz_fsdev_session_ctx *session_ctx)
{
	struct tz_fsdev_packet *packet = session_ctx->session_buf;
	int ret;

	packet->packet_size = wp_reply->size + sizeof(struct tz_fsdev_packet);

	if (copy_from_user(packet->buf, wp_reply->buf, wp_reply->size))
		return -EFAULT;

	packet->flags = 0;
	ret = tz_iwsock_write(session_ctx->sd, packet, packet->packet_size, 0);
	if (ret < 0) {
		log_error(tzdev_nwfs, "Failed iwsock write (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int tz_fsdev_reply(struct tz_fsdev_wp_reply *wp_reply, struct tz_fsdev_session_ctx *session_ctx)
{
	struct tz_fsdev_packet *packet = session_ctx->session_buf;
	const unsigned int packet_header_size = sizeof(struct tz_fsdev_packet);
	unsigned int size = wp_reply->size;
	int pfn_count, max_pfn_count, total_pfn_count;
	sk_pfn_t *pfns;
	int ret = 0;

	if (session_ctx->pin_info.pfns) {
		log_debug(tzdev_nwfs, "Free pfns\n");
		tz_fsdev_free_pfns(&session_ctx->pin_info);
	}

	packet->flags = wp_reply->flags;
	if (tz_fsdev_get_pfns_and_pin(wp_reply->buf, size, &session_ctx->pin_info)) {
		packet->ret = -ENOMEM;
		ret = tz_iwsock_write(session_ctx->sd, packet, packet_header_size, 0);
		if (ret < 0) {
			log_error(tzdev_nwfs, "Failed iwsock write (%d)\n", ret);
			return ret;
		}
		return -ENOMEM;
	}
	pfns = session_ctx->pin_info.pfns;
	total_pfn_count = DIV_ROUND_UP(size, PAGE_SIZE);
	max_pfn_count = (session_ctx->session_buf_size - packet_header_size) / sizeof(sk_pfn_t);
	do {
		packet->flags |= BUF_CONTAINS_PFN;
		packet->ret = wp_reply->ret;
		pfn_count = total_pfn_count >= max_pfn_count ? max_pfn_count : total_pfn_count;
		packet->packet_size = pfn_count * sizeof(sk_pfn_t) + packet_header_size;
		memcpy(packet->buf, pfns, pfn_count * sizeof(sk_pfn_t));
		ret = tz_iwsock_write(session_ctx->sd, packet, packet->packet_size, 0);
		if (ret < 0) {
			log_error(tzdev_nwfs, "Failed iwsock write (%d)\n", ret);
			return ret;
		}
		pfns += pfn_count;
		total_pfn_count -= pfn_count;
		if (total_pfn_count) {
			ret = tz_iwsock_read(session_ctx->sd, packet,
					session_ctx->session_buf_size, 0);
			if (ret < 0) {
				log_error(tzdev_nwfs, "Failed iwsock read (%d)\n", ret);
				return ret;
			}
			if (ret == 0) {
				log_error(tzdev_nwfs, LOG_KTHREAD "NWFS socket is unxpectedly closed.\n");
				return -ENODEV;
			}
			if (packet->cmd != NWFS_CMD_READ_FILE_CONTINUE)
				return -EINVAL;
		}
	} while (total_pfn_count);

	return 0;
}

static int tz_fsdev_reply_wp_command(struct file *file, struct tz_fsdev_wp_reply *wp_reply)
{
	struct tz_fsdev_session_ctx *session_ctx = file->private_data;
	struct tz_fsdev_packet *packet = session_ctx->session_buf;
	const int packet_header_size = sizeof(struct tz_fsdev_packet);
	int ret;

	packet->ret = wp_reply->ret;
	if (wp_reply->ret < 0) {
		ret = tz_iwsock_write(session_ctx->sd, packet, packet_header_size, 0);
		if (ret < 0)
			log_error(tzdev_nwfs, "Failed iwsock write (%d)\n", ret);

		return ret;
	}

	if (wp_reply->size <= (session_ctx->session_buf_size - packet_header_size))
		return tz_fsdev_short_reply(wp_reply, session_ctx);
	else
		return tz_fsdev_reply(wp_reply, session_ctx);
}

static int tz_fsdev_release(struct inode *inode, struct file *filp)
{
	if (filp->private_data)
		tz_iwsock_release(filp->private_data);

	return 0;
}

static int tz_fsdev_wp_release(struct inode *inode, struct file *filp)
{
	release_session_ctx(filp->private_data);

	return 0;
}

static int tz_fsdev_get_wp_command(struct file *file, void __user *ubuf)
{
	struct tz_fsdev_session_ctx *session_ctx = file->private_data;
	struct tz_fsdev_packet *packet = session_ctx->session_buf;
	ssize_t read_cnt;
	struct tz_msghdr msg;
	struct tz_cmsghdr_swd_cred msg_cred;
	struct tz_fsdev_data *data = ubuf;

	msg.msgbuf = (char *)packet;
	msg.msglen = session_ctx->session_buf_size;
	msg.msg_control = (char *)&msg_cred;
	msg.msg_controllen = sizeof(msg_cred);
	msg.msg_flags = 1;
	read_cnt = tz_iwsock_read_msg(session_ctx->sd, &msg, 1);
	if (read_cnt < 0) {
		log_error(tzdev_nwfs, "tz_iwsock_read ERROR. read_cnt = %d\n", (int)read_cnt);
		return read_cnt;
	}

	if (read_cnt < sizeof(struct tz_fsdev_packet)) {
		if (read_cnt == 0) {
			log_error(tzdev_nwfs, LOG_KTHREAD "NWFS socket is unxpectedly closed. NWFS is stopped.\n");
			return -ENODEV;
		}
		log_error(tzdev_nwfs, LOG_KTHREAD "read incorrect command.\n");
		return -EPIPE;
	}
	if (read_cnt != packet->packet_size || packet->cmd > NWFS_CMD_CNT) {
		log_error(tzdev_nwfs, LOG_KTHREAD "Wrong command:read_cnt = %d, packet->cmd = %d\n", (int)read_cnt, packet->cmd);
		return -EPIPE;
	}
	if (copy_to_user(&data->packet, packet, packet->packet_size))
		return -EFAULT;

	if (copy_to_user(&data->uuid, &msg_cred.cred.uuid, sizeof(struct tz_uuid)))
		return -EFAULT;

	return 0;
}

#ifdef CONFIG_COMPAT
static long compat_tz_fsdev_wp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tz_fsdev_wp_reply wp_reply;
	struct compat_tz_fsdev_wp_reply compat_wp_reply;

	switch (cmd) {
	case TZ_FSDEV_GET_WP_CMD:
		return tz_fsdev_get_wp_command(file, compat_ptr(arg));
	case COMPAT_TZ_FSDEV_REPLY_WP_CMD:
		if (copy_from_user(&compat_wp_reply, compat_ptr(arg),
				sizeof(struct compat_tz_fsdev_wp_reply)))
			return -EFAULT;
		wp_reply.ret = compat_wp_reply.ret;
		wp_reply.size = compat_wp_reply.size;
		wp_reply.flags = compat_wp_reply.flags;
		wp_reply.buf = compat_ptr(compat_wp_reply.buf);

		return tz_fsdev_reply_wp_command(file, &wp_reply);
	default:
		log_error(tzdev_nwfs, "Unexpected command %d from %ld pid\n", cmd, arg);
		return -ENOTTY;
	}

	return 0;
}
#endif /* CONFIG_COMPAT */

static long tz_fsdev_wp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tz_fsdev_wp_reply wp_reply;

	switch (cmd) {
	case TZ_FSDEV_GET_WP_CMD:
		return tz_fsdev_get_wp_command(file, (void __user *)arg);
	case TZ_FSDEV_REPLY_WP_CMD:
		if (copy_from_user(&wp_reply, (struct tz_fsdev_wp_reply __user *)arg,
				sizeof(struct tz_fsdev_wp_reply)))
			return -EFAULT;

		return tz_fsdev_reply_wp_command(file, &wp_reply);
	default:
		log_error(tzdev_nwfs, "Unexpected command %d from %ld pid\n", cmd, arg);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations tz_fsdev_wp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tz_fsdev_wp_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_tz_fsdev_wp_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
	.release = tz_fsdev_wp_release,
};

static long tz_fsdev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	pid_t cur_pid = current->pid;
	struct tz_fsdev_session_ctx *session_ctx;
	struct sock_desc *fsdev_sock;
	int optval, optlen;
	struct tz_fsdev_accept_data accept_data;

	switch (cmd) {
	case TZ_FSDEV_INIT:
		fsdev_sock = tz_iwsock_socket(1, TZ_INTERRUPTIBLE);
		if (IS_ERR_OR_NULL(fsdev_sock)) {
			log_error(tzdev_nwfs, "init socket ERROR = %ld\n", PTR_ERR(fsdev_sock));
			return PTR_ERR(fsdev_sock);
		}

		ret = tz_iwsock_listen(fsdev_sock, NWFS_CMD_SOCK);
		if (ret) {
			log_error(tzdev_nwfs, "listen socket ERROR = %d\n", ret);
			tz_iwsock_release(fsdev_sock);
			return ret;
		}

		file->private_data = fsdev_sock;
		break;
	case TZ_FSDEV_ACCEPT:
		if (!file->private_data) {
			log_error(tzdev_nwfs, "Socket is not created\n");
			return -EINVAL;
		}
		session_ctx = create_session_ctx();
		if (!session_ctx) {
			log_error(tzdev_nwfs, "Failed to allocate session context\n");
			return -ENOMEM;
		}

		session_ctx->sd = tz_iwsock_accept(file->private_data);
		if (IS_ERR_OR_NULL(session_ctx->sd)) {
			ret = PTR_ERR(session_ctx->sd);
			goto free_session_ctx;
		}

		/* Due to socket implementation specific we can't get socket buffer size by using
		 * socket API. Socket buffer size is bigger than the size that was set in SWD due to
		 * additional internal socket data.
		 */
		tz_iwsock_getsockopt(session_ctx->sd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);

		session_ctx->session_buf_size = accept_data.buf_size = round_down(optval, PAGE_SIZE);
		session_ctx->session_buf = kmalloc(session_ctx->session_buf_size, GFP_KERNEL);
		if (!session_ctx) {
			log_error(tzdev_nwfs, "Failed to allocate session buffer\n");
			ret = -ENOMEM;
			goto release_socket;
		}
		accept_data.fd = anon_inode_getfd("[nwfsdev-wp]", &tz_fsdev_wp_fops, session_ctx,
				O_CLOEXEC);
		if (accept_data.fd < 0) {
			log_error(tzdev_nwfs, "Failed to get new anon inode fd, ret=%d\n", accept_data.fd);
			ret = accept_data.fd;
			goto free_session_buf;
		}

		if (copy_to_user((void *)arg, &accept_data, sizeof(struct tz_fsdev_accept_data))) {
			ret = -EFAULT;
			goto free_session_buf;
		}
		return 0;

free_session_buf:
		kfree(session_ctx->session_buf);
		session_ctx->session_buf_size = 0;
release_socket:
		tz_iwsock_release(session_ctx->sd);
free_session_ctx:
		kfree(session_ctx);
		return ret;
	default:
		log_error(tzdev_nwfs, "Unexpected command %d from %d pid\n", cmd, cur_pid);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations tz_fsdev_fops = {
	.owner = THIS_MODULE,
	.open = tz_fsdev_open,
	.unlocked_ioctl = tz_fsdev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_fsdev_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
	.release = tz_fsdev_release,
};

static struct tz_cdev tz_fsdev_cdev = {
	.name = "nwfsdev",
	.fops = &tz_fsdev_fops,
	.owner = THIS_MODULE,
};

static int __init tz_fsdev_init(void)
{
	int rc;

	rc = tz_cdev_register(&tz_fsdev_cdev);
	if (rc)
		return rc;

	nwfs_daemon_ctx = create_session_ctx();
	if (!nwfs_daemon_ctx) {
		tz_cdev_unregister(&tz_fsdev_cdev);
		log_error(tzdev_nwfs, "NWFS Daemon context is not created. NWFS dev is not initialized\n");
		return -EFAULT;
	}

	return 0;
}

static void __exit tz_fsdev_exit(void)
{
	tz_cdev_unregister(&tz_fsdev_cdev);
	release_session_ctx(nwfs_daemon_ctx);
}

module_init(tz_fsdev_init);
module_exit(tz_fsdev_exit);
