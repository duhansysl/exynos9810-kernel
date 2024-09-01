/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_queue.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_queue.h"

#include "s5p_mfc_utils.h"
#include "s5p_mfc_mem.h"

void s5p_mfc_add_tail_buf(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		struct s5p_mfc_buf *mfc_buf)
{
	unsigned long flags;

	if (!mfc_buf) {
		mfc_err_dev("mfc_buf is NULL!\n");
		return;
	}

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "queue address: %p\n", queue);
	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);

	mfc_buf->used = 0;
	list_add_tail(&mfc_buf->list, &queue->head);
	queue->count++;

	spin_unlock_irqrestore(plock, flags);
}

int s5p_mfc_peek_buf_csd(spinlock_t *plock, struct s5p_mfc_buf_queue *queue)
{
	unsigned long flags;
	int csd = -1;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&queue->head)) {
		mfc_debug(2, "queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return csd;
	}

	mfc_buf = list_entry(queue->head.next, struct s5p_mfc_buf, list);

	csd = mfc_buf->vb.reserved2 & FLAG_CSD ? 1 : 0;

	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

	spin_unlock_irqrestore(plock, flags);
	return csd;
}

struct s5p_mfc_buf *s5p_mfc_get_buf(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		enum s5p_mfc_queue_used_type used)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&queue->head)) {
		mfc_debug(2, "queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(queue->head.next, struct s5p_mfc_buf, list);

	if ((used == MFC_BUF_RESET_USED) || (used == MFC_BUF_SET_USED))
		mfc_buf->used = used;

	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

	spin_unlock_irqrestore(plock, flags);
	return mfc_buf;
}

struct s5p_mfc_buf *s5p_mfc_get_del_buf(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		enum s5p_mfc_queue_used_type used)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&queue->head)) {
		mfc_debug(2, "queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(queue->head.next, struct s5p_mfc_buf, list);

	if ((used == MFC_BUF_RESET_USED) || (used == MFC_BUF_SET_USED))
		mfc_buf->used = used;

	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

	list_del(&mfc_buf->list);
	queue->count--;

	spin_unlock_irqrestore(plock, flags);
	return mfc_buf;
}

struct s5p_mfc_buf *s5p_mfc_get_del_if_consumed(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		unsigned long consumed, unsigned int min_bytes, int error, int *deleted)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	unsigned int remained;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&queue->head)) {
		mfc_debug(2, "queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(queue->head.next, struct s5p_mfc_buf, list);

	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

	remained = (unsigned int)(mfc_buf->vb.vb2_buf.planes[0].bytesused - consumed);

	mfc_debug(2, "Packed PB test. Total Size: %d, consumed: %ld, remained: %d\n",
		mfc_buf->vb.vb2_buf.planes[0].bytesused, consumed, remained);

	if ((consumed > 0) && (remained > min_bytes) && (error == 0) &&
			(mfc_buf->vb.vb2_buf.planes[0].bytesused > consumed)){
		/* do not delete from queue */
		*deleted = 0;
	} else {
		list_del(&mfc_buf->list);
		queue->count--;

		*deleted = 1;
	}

	spin_unlock_irqrestore(plock, flags);
	return mfc_buf;
}

struct s5p_mfc_buf *s5p_mfc_get_move_buf(spinlock_t *plock,
		struct s5p_mfc_buf_queue *to_queue, struct s5p_mfc_buf_queue *from_queue,
		enum s5p_mfc_queue_used_type used, enum s5p_mfc_queue_top_type top)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&from_queue->head)) {
		mfc_debug(2, "from_queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(from_queue->head.next, struct s5p_mfc_buf, list);

	if ((used == MFC_BUF_RESET_USED) || (used == MFC_BUF_SET_USED))
		mfc_buf->used = used;

	mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

	list_del(&mfc_buf->list);
	from_queue->count--;

	if (top == MFC_QUEUE_ADD_TOP)
		list_add(&mfc_buf->list, &to_queue->head);
	else
		list_add_tail(&mfc_buf->list, &to_queue->head);

	to_queue->count++;

	spin_unlock_irqrestore(plock, flags);
	return mfc_buf;
}

struct s5p_mfc_buf *s5p_mfc_get_move_buf_used(spinlock_t *plock,
		struct s5p_mfc_buf_queue *to_queue, struct s5p_mfc_buf_queue *from_queue)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&from_queue->head)) {
		mfc_debug(2, "from_queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(from_queue->head.next, struct s5p_mfc_buf, list);

	if (mfc_buf->used) {
		mfc_debug(2, "mfc_buf: %p\n", mfc_buf);
		mfc_debug(2, "First plane address: 0x%08llx\n",
			s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

		list_del(&mfc_buf->list);
		from_queue->count--;

		list_add_tail(&mfc_buf->list, &to_queue->head);
		to_queue->count++;

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

struct s5p_mfc_buf *s5p_mfc_get_move_buf_addr(spinlock_t *plock,
		struct s5p_mfc_buf_queue *to_queue, struct s5p_mfc_buf_queue *from_queue,
		dma_addr_t addr)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&from_queue->head)) {
		mfc_debug(2, "from_queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}

	mfc_buf = list_entry(from_queue->head.next, struct s5p_mfc_buf, list);

	if (s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0) == addr) {
		mfc_debug(2, "First plane address: 0x%08llx\n",
				s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0));

		list_del(&mfc_buf->list);
		from_queue->count--;

		list_add_tail(&mfc_buf->list, &to_queue->head);
		to_queue->count++;

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

struct s5p_mfc_buf *s5p_mfc_find_buf_vb(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		dma_addr_t addr)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	dma_addr_t mb_addr;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for this address: 0x%08llx\n", addr);
	list_for_each_entry(mfc_buf, &queue->head, list) {
		mb_addr = s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0);
		mfc_debug(2, "plane[0] addr: 0x%08llx\n", mb_addr);

		if (addr == mb_addr) {
			spin_unlock_irqrestore(plock, flags);
			return mfc_buf;
		}
	}

	spin_unlock_irqrestore(plock, flags);
	return NULL;
}

struct s5p_mfc_buf *s5p_mfc_find_del_buf_raw(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		dma_addr_t addr)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	dma_addr_t mb_addr;
	int found = 0;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for this address: 0x%08llx\n", addr);
	list_for_each_entry(mfc_buf, &queue->head, list) {
		mb_addr = mfc_buf->planes.raw[0];
		mfc_debug(2, "plane[0] addr: 0x%08llx\n", mb_addr);

		if (addr == mb_addr) {
			found = 1;
			break;
		}
	}

	if (found == 1) {
		list_del(&mfc_buf->list);
		queue->count--;

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

struct s5p_mfc_buf *s5p_mfc_find_del_buf_vb(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		dma_addr_t addr)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	dma_addr_t mb_addr;
	int found = 0;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for this address: 0x%08llx\n", addr);
	list_for_each_entry(mfc_buf, &queue->head, list) {
		mb_addr = s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0);
		mfc_debug(2, "plane[0] addr: 0x%08llx\n", mb_addr);

		if (addr == mb_addr) {
			found = 1;
			break;
		}
	}

	if (found == 1) {
		list_del(&mfc_buf->list);
		queue->count--;

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

struct s5p_mfc_buf *s5p_mfc_find_move_buf_vb(spinlock_t *plock,
		struct s5p_mfc_buf_queue *to_queue, struct s5p_mfc_buf_queue *from_queue,
		dma_addr_t addr, unsigned int released_flag)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	dma_addr_t mb_addr;
	int found = 0;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for this address: 0x%08llx\n", addr);
	list_for_each_entry(mfc_buf, &from_queue->head, list) {
		mb_addr = s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0);
		mfc_debug(2, "plane[0] addr: 0x%08llx\n", mb_addr);

		if (addr == mb_addr) {
			found = 1;
			break;
		}
	}

	if (found == 1) {
		if (released_flag & (1 << mfc_buf->vb.vb2_buf.index)) {
			list_del(&mfc_buf->list);
			from_queue->count--;

			list_add_tail(&mfc_buf->list, &to_queue->head);
			to_queue->count++;
		}

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

struct s5p_mfc_buf *s5p_mfc_find_move_buf_vb_used(spinlock_t *plock,
		struct s5p_mfc_buf_queue *to_queue, struct s5p_mfc_buf_queue *from_queue,
		dma_addr_t addr)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	dma_addr_t mb_addr;
	int found = 0;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for this address: 0x%08llx\n", addr);
	list_for_each_entry(mfc_buf, &from_queue->head, list) {
		mb_addr = s5p_mfc_mem_get_daddr_vb(&mfc_buf->vb.vb2_buf, 0);
		mfc_debug(2, "plane[0] addr: 0x%08llx, used: %d\n",
				mb_addr, mfc_buf->used);

		if ((addr == mb_addr) && (mfc_buf->used == 1)) {
			found = 1;
			break;
		}
	}

	if (found == 1) {
		list_del(&mfc_buf->list);
		from_queue->count--;

		list_add_tail(&mfc_buf->list, &to_queue->head);
		to_queue->count++;

		spin_unlock_irqrestore(plock, flags);
		return mfc_buf;
	} else {
		spin_unlock_irqrestore(plock, flags);
		return NULL;
	}
}

void s5p_mfc_move_first_buf_used(spinlock_t *plock, struct s5p_mfc_buf_queue *to_queue,
		struct s5p_mfc_buf_queue *from_queue, enum s5p_mfc_queue_top_type top)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (list_empty(&from_queue->head)) {
		mfc_err_dev("from_queue is empty\n");
		spin_unlock_irqrestore(plock, flags);
		return;
	}
	mfc_buf = list_entry(from_queue->head.next, struct s5p_mfc_buf, list);

	if (mfc_buf->used) {
		list_del(&mfc_buf->list);
		from_queue->count--;

		if (top == MFC_QUEUE_ADD_TOP)
			list_add(&mfc_buf->list, &to_queue->head);
		else
			list_add_tail(&mfc_buf->list, &to_queue->head);

		to_queue->count++;
	}

	spin_unlock_irqrestore(plock, flags);
}

void s5p_mfc_move_all_bufs(spinlock_t *plock, struct s5p_mfc_buf_queue *to_queue,
		struct s5p_mfc_buf_queue *from_queue, enum s5p_mfc_queue_top_type top)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(plock, flags);

	if (top == MFC_QUEUE_ADD_TOP) {
		while (!list_empty(&from_queue->head)) {
			mfc_buf = list_entry(from_queue->head.prev, struct s5p_mfc_buf, list);

			list_del(&mfc_buf->list);
			from_queue->count--;

			list_add(&mfc_buf->list, &to_queue->head);
			to_queue->count++;
		}
	} else {
		while (!list_empty(&from_queue->head)) {
			mfc_buf = list_entry(from_queue->head.next, struct s5p_mfc_buf, list);

			list_del(&mfc_buf->list);
			from_queue->count--;

			list_add_tail(&mfc_buf->list, &to_queue->head);
			to_queue->count++;
		}
	}

	INIT_LIST_HEAD(&from_queue->head);
	from_queue->count = 0;

	spin_unlock_irqrestore(plock, flags);
}

void s5p_mfc_cleanup_queue(spinlock_t *plock, struct s5p_mfc_buf_queue *queue)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	int i;

	spin_lock_irqsave(plock, flags);

	while (!list_empty(&queue->head)) {
		mfc_buf = list_entry(queue->head.next, struct s5p_mfc_buf, list);

		for (i = 0; i < mfc_buf->vb.vb2_buf.num_planes; i++)
			vb2_set_plane_payload(&mfc_buf->vb.vb2_buf, i, 0);
		vb2_buffer_done(&mfc_buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
		list_del(&mfc_buf->list);
		queue->count--;
	}

	INIT_LIST_HEAD(&queue->head);
	queue->count = 0;

	spin_unlock_irqrestore(plock, flags);
}

struct s5p_mfc_buf *mfc_find_buf_index(spinlock_t *plock, struct s5p_mfc_buf_queue *queue,
		int index)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	int buf_index;

	spin_lock_irqsave(plock, flags);

	mfc_debug(2, "Looking for index: %d\n", index);
	list_for_each_entry(mfc_buf, &queue->head, list) {
		buf_index = mfc_buf->vb.vb2_buf.index;

		if (index == buf_index) {
			mfc_debug(2, "Found index: %d\n", buf_index);
			spin_unlock_irqrestore(plock, flags);
			return mfc_buf;
		}
	}

	spin_unlock_irqrestore(plock, flags);
	return NULL;
}

/*
 * Check released and enqueued buffers. (dst queue)
 * Check and move reuse buffers. (ref -> dst queue)
 */
static void mfc_check_ref_frame(spinlock_t *plock, struct s5p_mfc_buf_queue *dst_queue,
		struct s5p_mfc_buf_queue *ref_queue,
		struct s5p_mfc_ctx *ctx, int ref_index)
{
	unsigned long flags;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *ref_mb, *tmp_mb;
	int index;
	int found = 0;

	spin_lock_irqsave(plock, flags);

	/* reuse buffers : error frame, decoding only frame (ref -> dst queue) */
	list_for_each_entry_safe(ref_mb, tmp_mb, &ref_queue->head, list) {
		index = ref_mb->vb.vb2_buf.index;
		if (index == ref_index) {
			list_del(&ref_mb->list);
			ref_queue->count--;

			list_add_tail(&ref_mb->list, &dst_queue->head);
			dst_queue->count++;

			dec->assigned_fd[index] =
					ref_mb->vb.vb2_buf.planes[0].m.fd;
			clear_bit(index, &dec->available_dpb);
			mfc_debug(2, "Move buffer[%d], fd[%d] to dst queue\n",
					index, dec->assigned_fd[index]);
			found = 1;
			break;
		}
	}

	/* released and enqueued buffers (dst queue) */
	if (!found) {
		list_for_each_entry_safe(ref_mb, tmp_mb, &dst_queue->head, list) {
			index = ref_mb->vb.vb2_buf.index;
			if (index == ref_index) {
				dec->assigned_fd[index] =
					ref_mb->vb.vb2_buf.planes[0].m.fd;
				clear_bit(index, &dec->available_dpb);
				mfc_debug(2, "re-assigned buffer[%d], fd[%d]\n",
						index, dec->assigned_fd[index]);
				break;
			}
		}
	}

	spin_unlock_irqrestore(plock, flags);
}

/* Process the released reference information */
void s5p_mfc_handle_released_info(struct s5p_mfc_ctx *ctx,
		unsigned int released_flag, int index)
{
	struct s5p_mfc_dec *dec;
	struct dec_dpb_ref_info *refBuf;
	int t, ncount = 0;

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err_dev("no decoder context to run\n");
		return;
	}
	refBuf = &dec->ref_info[index];

	if (dec->dec_only_release_flag) {
		for (t = 0; t < MFC_MAX_DPBS; t++) {
			if (dec->dec_only_release_flag & (1 << t)) {
				mfc_debug(2, "Release FD[%d] = %03d (already released in dec only)\n",
						t, dec->assigned_fd[t]);
				refBuf->dpb[ncount].fd[0] = dec->assigned_fd[t];
				ncount++;
				dec->dec_only_release_flag &= ~(1 << t);
			}
		}
	}

	if (released_flag) {
		for (t = 0; t < MFC_MAX_DPBS; t++) {
			if (released_flag & (1 << t)) {
				if (dec->err_reuse_flag & (1 << t)) {
					/* reuse buffer with error : do not update released info */
					mfc_debug(2, "Released, but reuse(error frame). FD[%d] = %03d\n",
							t, dec->assigned_fd[t]);
					dec->err_reuse_flag &= ~(1 << t);
				} else if ((t != index) &&
						mfc_find_buf_index(&ctx->buf_queue_lock, &ctx->ref_buf_queue, t)) {
					/* decoding only frame: do not update released info */
					mfc_debug(2, "Released, but reuse(decoding only). FD[%d] = %03d\n",
							t, dec->assigned_fd[t]);
				} else {
					/* displayed and released frame */
					mfc_debug(2, "Release FD[%d] = %03d\n",
							t, dec->assigned_fd[t]);
					refBuf->dpb[ncount].fd[0] = dec->assigned_fd[t];
					ncount++;
				}
				dec->assigned_fd[t] = MFC_INFO_INIT_FD;
				mfc_check_ref_frame(&ctx->buf_queue_lock, &ctx->dst_buf_queue,
						&ctx->ref_buf_queue, ctx, t);
			}
		}
	}

	if (ncount != MFC_MAX_DPBS)
		refBuf->dpb[ncount].fd[0] = MFC_INFO_INIT_FD;
}

struct s5p_mfc_buf *s5p_mfc_move_reuse_buffer(struct s5p_mfc_ctx *ctx, int release_index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf_queue *dst_queue = &ctx->dst_buf_queue;
	struct s5p_mfc_buf_queue *ref_queue = &ctx->ref_buf_queue;
	struct s5p_mfc_buf *ref_mb, *tmp_mb;
	spinlock_t *plock = &ctx->buf_queue_lock;
	unsigned long flags;
	int index;

	spin_lock_irqsave(plock, flags);

	list_for_each_entry_safe(ref_mb, tmp_mb, &ref_queue->head, list) {
		index = ref_mb->vb.vb2_buf.index;
		if (index == release_index) {
			s5p_mfc_raw_unprotect(ctx, ref_mb, index);

			ref_mb->used = 0;

			list_del(&ref_mb->list);
			ref_queue->count--;

			list_add_tail(&ref_mb->list, &dst_queue->head);
			dst_queue->count++;

			clear_bit(index, &dec->available_dpb);
			mfc_debug(2, "buffer[%d] is moved to dst queue for reuse\n", index);

			spin_unlock_irqrestore(plock, flags);
			return ref_mb;
		}
	}

	spin_unlock_irqrestore(plock, flags);
	return NULL;
}

void s5p_mfc_cleanup_enc_src_queue(struct s5p_mfc_ctx *ctx)
{
	unsigned long flags;
	struct s5p_mfc_buf *src_mb, *tmp_mb;
	int i;

	if (ctx->is_drm && ctx->raw_protect_flag) {
		spin_lock_irqsave(&ctx->buf_queue_lock, flags);

		mfc_debug(2, "raw_protect_flag(%#lx) will be released\n",
				ctx->raw_protect_flag);

		list_for_each_entry_safe(src_mb, tmp_mb, &ctx->src_buf_queue.head, list) {
			i = src_mb->vb.vb2_buf.index;

			s5p_mfc_raw_unprotect(ctx, src_mb, i);

			for (i = 0; i < src_mb->vb.vb2_buf.num_planes; i++)
				vb2_set_plane_payload(&src_mb->vb.vb2_buf, i, 0);

			vb2_buffer_done(&src_mb->vb.vb2_buf, VB2_BUF_STATE_ERROR);
			list_del(&src_mb->list);
			ctx->src_buf_queue.count--;
		}

		INIT_LIST_HEAD(&ctx->src_buf_queue.head);
		ctx->src_buf_queue.count = 0;

		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	} else {
		s5p_mfc_cleanup_queue(&ctx->buf_queue_lock, &ctx->src_buf_queue);
	}
}


void s5p_mfc_cleanup_enc_dst_queue(struct s5p_mfc_ctx *ctx)
{
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb, *tmp_mb;
	int i;

	if (ctx->is_drm && ctx->stream_protect_flag) {
		spin_lock_irqsave(&ctx->buf_queue_lock, flags);

		mfc_debug(2, "stream_protect_flag(%#lx) will be released\n",
				ctx->stream_protect_flag);

		list_for_each_entry_safe(dst_mb, tmp_mb, &ctx->dst_buf_queue.head, list) {
			i = dst_mb->vb.vb2_buf.index;

			s5p_mfc_stream_unprotect(ctx, dst_mb, i);

			for (i = 0; i < dst_mb->vb.vb2_buf.num_planes; i++)
				vb2_set_plane_payload(&dst_mb->vb.vb2_buf, i, 0);
			vb2_buffer_done(&dst_mb->vb.vb2_buf, VB2_BUF_STATE_ERROR);
			list_del(&dst_mb->list);
			ctx->dst_buf_queue.count--;
		}

		INIT_LIST_HEAD(&ctx->dst_buf_queue.head);
		ctx->dst_buf_queue.count = 0;

		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
	} else {
		s5p_mfc_cleanup_queue(&ctx->buf_queue_lock, &ctx->dst_buf_queue);
	}
}

/* Check all buffers are referenced or not */
struct s5p_mfc_buf *mfc_check_full_refered_dpb(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec = NULL;
	struct s5p_mfc_buf *mfc_buf = NULL;
	int sum_dpb;

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err_dev("no decoder context to run\n");
		return NULL;
	}

	sum_dpb = ctx->dst_buf_queue.count + ctx->ref_buf_queue.count;

	if ((ctx->dst_buf_queue.count > 1) && (sum_dpb >= (ctx->dpb_count + 5))) {
		if (list_empty(&ctx->dst_buf_queue.head)) {
			mfc_err_ctx("dst_buf_queue is empty\n");
			return NULL;
		}
		mfc_debug(2, "We should use this buffer.\n");
		mfc_buf = list_entry(ctx->dst_buf_queue.head.next,
				struct s5p_mfc_buf, list);
	} else if ((ctx->dst_buf_queue.count == 0)
			&& ((ctx->ref_buf_queue.count) == (ctx->dpb_count + 5))) {
		if (list_empty(&ctx->ref_buf_queue.head)) {
			mfc_err_ctx("ref_buf_queue is empty\n");
			return NULL;
		}
		mfc_debug(2, "All buffers are referenced.\n");
		mfc_buf = list_entry(ctx->ref_buf_queue.head.next,
				struct s5p_mfc_buf, list);
	} else {
		mfc_debug(2, "waiting for dst buffer, ref = %d, dst = %d\n",
				ctx->ref_buf_queue.count, ctx->dst_buf_queue.count);
		ctx->clear_work_bit = 1;
	}

	if (mfc_buf)
		mfc_buf->used = 1;

	return mfc_buf;
}

/* Try to search non-referenced DPB on dst-queue */
struct s5p_mfc_buf *s5p_mfc_search_for_dpb(struct s5p_mfc_ctx *ctx, unsigned int dynamic_used)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);

	mfc_debug(2, "Try to find non-referenced DPB. dynamic_used: 0x%x\n", dynamic_used);
	list_for_each_entry(mfc_buf, &ctx->dst_buf_queue.head, list) {
		if ((dynamic_used & (1 << mfc_buf->vb.vb2_buf.index)) == 0) {
			mfc_buf->used = 1;
			spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
			return mfc_buf;
		}
	}

	mfc_buf = mfc_check_full_refered_dpb(ctx);

	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	return mfc_buf;
}

struct s5p_mfc_buf *s5p_mfc_search_move_dpb_nal_q(struct s5p_mfc_ctx *ctx, unsigned int dynamic_used)
{
	unsigned long flags;
	struct s5p_mfc_buf *mfc_buf = NULL;
	struct s5p_mfc_dec *dec = NULL;

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err_dev("no mfc decoder to run\n");
		return mfc_buf;
	}

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);

	mfc_debug(2, "Try to find non-referenced DPB. dynamic_used: 0x%x\n", dynamic_used);
	list_for_each_entry(mfc_buf, &ctx->dst_buf_queue.head, list) {
		if ((dynamic_used & (1 << mfc_buf->vb.vb2_buf.index)) == 0) {
			mfc_buf->used = 1;

			list_del(&mfc_buf->list);
			ctx->dst_buf_queue.count--;

			list_add_tail(&mfc_buf->list, &ctx->dst_buf_nal_queue.head);
			ctx->dst_buf_nal_queue.count++;

			spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
			return mfc_buf;
		}
	}

	mfc_buf = mfc_check_full_refered_dpb(ctx);

	if (mfc_buf) {
		mfc_debug(2, "DPB is full. stop NAL-Q if started\n");
		dec->is_dpb_full = 1;
	}

	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	return mfc_buf;
}

/* Add dst buffer in dst_buf_queue */
void s5p_mfc_store_dpb(struct s5p_mfc_ctx *ctx, struct vb2_buffer *vb)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_buf *mfc_buf;
	int index;

	if (!ctx) {
		mfc_err_dev("no mfc context to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err_dev("no mfc decoder to run\n");
		return;
	}

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);

	mfc_buf = vb_to_mfc_buf(vb);
	mfc_buf->used = 0;
	index = vb->index;

	dec->assigned_fd[index] = vb->planes[0].m.fd;
	mfc_debug(2, "Assigned FD[%d] = %d (%s)\n", index, dec->assigned_fd[index],
			(dec->dynamic_used & (1 << index) ? "used" : "non-used"));

	list_add_tail(&mfc_buf->list, &ctx->dst_buf_queue.head);
	ctx->dst_buf_queue.count++;

	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	mutex_lock(&dec->dpb_mutex);
	if (!dec->assigned_refcnt[index]) {
		mfc_debug(2, "[IOVMM] map the new DPB[%d] %#llx\n", index, mfc_buf->planes.raw[0]);
		s5p_mfc_get_iovmm(ctx, vb);
	} else {
		if (dec->assigned_refcnt[index] == 2) {
			mfc_debug(2, "[IOVMM] remove spare DPB[%d] %#llx\n",
					index, dec->assigned_addr[index][1][0]);
			s5p_mfc_put_iovmm(ctx, ctx->dst_fmt->mem_planes, index, 1);
		}

		if (dec->assigned_addr[index][0][0] != mfc_buf->planes.raw[0]) {
			if (dec->dynamic_used & (1 << index)) {
				mfc_debug(2, "[IOVMM] used DPB[%d] was changed %#llx->%#llx (used: %#x)\n",
						index, dec->assigned_addr[index][0][0],
						mfc_buf->planes.raw[0], dec->dynamic_used);
				MFC_TRACE_CTX("used DPB[%d] %#llx->%#llx (%#x)\n",
						index, dec->assigned_addr[index][0][0],
						mfc_buf->planes.raw[0], dec->dynamic_used);
				s5p_mfc_move_iovmm_to_spare(ctx, ctx->dst_fmt->mem_planes, index);
				s5p_mfc_get_iovmm(ctx, vb);
			} else {
				mfc_debug(2, "[IOVMM] DPB[%d] was changed %#llx->%#llx (used: %#x)\n",
						index, dec->assigned_addr[index][0][0],
						mfc_buf->planes.raw[0], dec->dynamic_used);
				MFC_TRACE_CTX("DPB[%d] %#llx->%#llx (%#x)\n",
						index, dec->assigned_addr[index][0][0],
						mfc_buf->planes.raw[0], dec->dynamic_used);
				s5p_mfc_put_iovmm(ctx, ctx->dst_fmt->mem_planes, index, 0);
				s5p_mfc_get_iovmm(ctx, vb);
			}
		} else {
			mfc_debug(2, "[IOVMM] DPB[%d] has same address %#llx and already mapped\n",
					index, mfc_buf->planes.raw[0]);
		}
	}
	mutex_unlock(&dec->dpb_mutex);
}

void s5p_mfc_cleanup_nal_queue(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *src_mb, *dst_mb;
	unsigned long flags;
	unsigned int index;

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);

	while (!list_empty(&ctx->src_buf_nal_queue.head)) {
		src_mb = list_entry(ctx->src_buf_nal_queue.head.prev, struct s5p_mfc_buf, list);

		index = src_mb->vb.vb2_buf.index;
		call_cop(ctx, restore_buf_ctrls, ctx, &ctx->src_ctrls[index]);

		src_mb->used = 0;

		list_del(&src_mb->list);
		ctx->src_buf_nal_queue.count--;

		list_add(&src_mb->list, &ctx->src_buf_queue.head);
		ctx->src_buf_queue.count++;

		mfc_debug(2, "NAL Q: cleanup, src_buf_nal_queue -> src_buf_queue, index:%d\n",
				src_mb->vb.vb2_buf.index);
	}

	while (!list_empty(&ctx->dst_buf_nal_queue.head)) {
		dst_mb = list_entry(ctx->dst_buf_nal_queue.head.prev, struct s5p_mfc_buf, list);

		dst_mb->used = 0;
		if (ctx->type == MFCINST_DECODER)
			clear_bit(dst_mb->vb.vb2_buf.index, &dec->available_dpb);
		list_del(&dst_mb->list);
		ctx->dst_buf_nal_queue.count--;

		list_add(&dst_mb->list, &ctx->dst_buf_queue.head);
		ctx->dst_buf_queue.count++;

		mfc_debug(2, "NAL Q: cleanup, dst_buf_nal_queue -> dst_buf_queue, index:%d\n",
				dst_mb->vb.vb2_buf.index);
	}

	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
}

int s5p_mfc_is_last_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_buf *src_mb;
	struct s5p_mfc_dev *dev;
	unsigned long flags;

	mfc_debug_enter();

	if (!ctx) {
		mfc_err_dev("no mfc context to run\n");
		return -EINVAL;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err_dev("no mfc device to run\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ctx->buf_queue_lock, flags);

	if (list_empty(&ctx->src_buf_queue.head)) {
		mfc_debug(2, "src_buf_queue is empty\n");
		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
		return -EINVAL;
	}

	src_mb = list_entry(ctx->src_buf_queue.head.next, struct s5p_mfc_buf, list);

	mfc_debug(2, "mfc_buf: %p\n", src_mb);
	mfc_debug(2, "First plane address: 0x%08llx\n",
		s5p_mfc_mem_get_daddr_vb(&src_mb->vb.vb2_buf, 0));

	if (src_mb->vb.reserved2 & FLAG_LAST_FRAME) {
		mfc_debug(2, "last frame!\n");
		spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);
		return 1;
	}

	mfc_debug(2, "not last frame!\n");
	spin_unlock_irqrestore(&ctx->buf_queue_lock, flags);

	mfc_debug_leave();

	return 0;
}
