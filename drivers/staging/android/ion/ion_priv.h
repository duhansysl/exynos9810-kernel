/*
 * drivers/staging/android/ion/ion_priv.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ION_PRIV_H
#define _ION_PRIV_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/shrinker.h>
#include <linux/types.h>
#include <linux/miscdevice.h>

#ifdef CONFIG_ION_POOL_CACHE_POLICY
#include <asm/cacheflush.h>
#endif
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/dma-direction.h>
#include <trace/events/ion.h>
#include <asm/cacheflush.h>

#include "ion.h"

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
#define ION_EVENT_LOG_MAX	1024
#define ION_EVENT_BEGIN()	ktime_t begin = ktime_get()
#define ION_EVENT_DONE()	begin

typedef enum ion_event_type {
	ION_EVENT_TYPE_ALLOC = 0,
	ION_EVENT_TYPE_FREE,
	ION_EVENT_TYPE_MMAP,
	ION_EVENT_TYPE_SHRINK,
	ION_EVENT_TYPE_CLEAR,
} ion_event_t;

#define ION_EVENT_HEAPNAME	8
struct ion_event_alloc {
	void *id;
	unsigned char heapname[ION_EVENT_HEAPNAME];
	size_t size;
	unsigned long flags;
};

struct ion_event_free {
	void *id;
	unsigned char heapname[ION_EVENT_HEAPNAME];
	size_t size;
	bool shrinker;
};

struct ion_event_mmap {
	void *id;
	unsigned char heapname[ION_EVENT_HEAPNAME];
	size_t size;
};

struct ion_event_shrink {
	size_t size;
};

struct ion_event_clear {
	void *id;
	unsigned char heapname[ION_EVENT_HEAPNAME];
	size_t size;
	unsigned long flags;
};

struct ion_eventlog {
	ion_event_t type;
	union {
		struct ion_event_alloc alloc;
		struct ion_event_free free;
		struct ion_event_mmap mmap;
		struct ion_event_shrink shrink;
		struct ion_event_clear clear;
	} data;
	ktime_t begin;
	ktime_t done;
};

void ION_EVENT_SHRINK(struct ion_device *dev, size_t size);
void ION_EVENT_CLEAR(struct ion_buffer *buffer, ktime_t begin);
#else
#define ION_EVENT_BEGIN()		do { } while (0)
#define ION_EVENT_DONE()		do { } while (0)
#define ION_EVENT_SHRINK(dev, size)	do { } while (0)
#define ION_EVENT_CLEAR(buffer, begin)	do { } while (0)
#endif

struct ion_buffer *ion_handle_buffer(struct ion_handle *handle);

struct ion_buffer_prot_info {
	unsigned int chunk_count;
	unsigned int dma_addr;
	unsigned int flags;
	unsigned int chunk_size;
	unsigned long bus_address; /* physical address of phys. address array */
};

struct ion_buffer_info {
	struct ion_buffer_prot_info prot_desc;
	struct sg_table table;
};

struct ion_iovm_map {
	struct list_head list;
	struct device *dev;
	struct iommu_domain *domain;
	dma_addr_t iova;
	int prop;
};

/**
 * struct ion_buffer - metadata for a particular buffer
 * @ref:		reference count
 * @node:		node in the ion_device buffers tree
 * @dev:		back pointer to the ion_device
 * @heap:		back pointer to the heap the buffer came from
 * @flags:		buffer specific flags
 * @private_flags:	internal buffer specific flags
 * @size:		size of the buffer
 * @priv_virt:		private data to the buffer representable as
 *			a void *
 * @lock:		protects the buffers cnt fields
 * @kmap_cnt:		number of times the buffer is mapped to the kernel
 * @vaddr:		the kernel mapping if kmap_cnt is not zero
 * @dmap_cnt:		number of times the buffer is mapped for dma
 * @sg_table:		the sg table for the buffer if dmap_cnt is not zero
 * @pages:		flat array of pages in the buffer -- used by fault
 *			handler and only valid for buffers that are faulted in
 * @vmas:		list of vma's mapping this buffer
 * @handle_count:	count of handles referencing this buffer
 * @task_comm:		taskcomm of last client to reference this buffer in a
 *			handle, used for debugging
 * @pid:		pid of last client to reference this buffer in a
 *			handle, used for debugging
*/
struct ion_buffer {
	struct kref ref;
	union {
		struct rb_node node;
		struct list_head list;
	};
	struct ion_device *dev;
	struct ion_heap *heap;
	unsigned long flags;
	unsigned long private_flags;
	size_t size;
	void *priv_virt;
	struct mutex lock;
	int kmap_cnt;
	void *vaddr;
	int dmap_cnt;
	struct sg_table *sg_table;
	struct page **pages;
	struct list_head vmas;
	struct list_head iovas;
	/* used to track orphaned buffers */
	int handle_count;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
	struct list_head master_list;
	char thread_comm[TASK_COMM_LEN];
	pid_t tid;
#endif
};

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
struct ion_task {
	struct list_head list;
	struct kref ref;
	struct device *master;
};
#endif

void ion_buffer_destroy(struct ion_buffer *buffer);

/**
 * struct ion_device - the metadata of the ion device node
 * @dev:		the actual misc device
 * @buffers:		an rb tree of all the existing buffers
 * @buffer_lock:	lock protecting the tree of buffers
 * @lock:		rwsem protecting the tree of heaps and clients
 * @heaps:		list of all the heaps in the system
 * @user_clients:	list of all the clients created from userspace
 */
struct ion_device {
	struct miscdevice dev;
	struct rb_root buffers;
	struct mutex buffer_lock;
	struct rw_semaphore lock;
	struct plist_head heaps;
	long (*custom_ioctl)(struct ion_client *client, unsigned int cmd,
			     unsigned long arg);
	struct rb_root clients;
	struct dentry *debug_root;
	struct dentry *heaps_debug_root;
	struct dentry *clients_debug_root;

#ifdef CONFIG_ION_EXYNOS_STAT_LOG
	/* event log */
	struct dentry *buffer_debug_file;
	struct dentry *event_debug_file;
	struct ion_eventlog eventlog[ION_EVENT_LOG_MAX];
	atomic_t event_idx;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	struct proc_dir_entry *proc_root;
	struct proc_dir_entry *heaps_proc_root;
	struct proc_dir_entry *clients_proc_root;
#endif
	int heap_cnt;
};

/**
 * struct ion_client - a process/hw block local address space
 * @node:		node in the tree of all clients
 * @dev:		backpointer to ion device
 * @handles:		an rb tree of all the handles in this client
 * @idr:		an idr space for allocating handle ids
 * @lock:		lock protecting the tree of handles
 * @name:		used for debugging
 * @display_name:	used for debugging (unique version of @name)
 * @display_serial:	used for debugging (to make display_name unique)
 * @task:		used for debugging
 *
 * A client represents a list of buffers this client may access.
 * The mutex stored here is used to protect both handles tree
 * as well as the handles themselves, and should be held while modifying either.
 */
struct ion_client {
	struct rb_node node;
	struct ion_device *dev;
	struct rb_root handles;
	struct idr idr;
	struct mutex lock;
	const char *name;
	char *display_name;
	int display_serial;
	struct task_struct *task;
	pid_t pid;
	struct dentry *debug_root;
#if IS_ENABLED(CONFIG_PROC_FS)
	struct proc_dir_entry *proc_root;
#endif
};

/**
 * ion_handle - a client local reference to a buffer
 * @ref:		reference count
 * @client:		back pointer to the client the buffer resides in
 * @buffer:		pointer to the buffer
 * @node:		node in the client's handle rbtree
 * @kmap_cnt:		count of times this client has mapped to kernel
 * @id:			client-unique id allocated by client->idr
 *
 * Modifications to node, map_cnt or mapping should be protected by the
 * lock in the client.  Other fields are never changed after initialization.
 */
struct ion_handle {
	struct kref ref;
	unsigned int user_ref_count;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

/**
 * struct ion_heap_ops - ops to operate on a given heap
 * @allocate:		allocate memory
 * @free:		free memory
 * @map_kernel		map memory to the kernel
 * @unmap_kernel	unmap memory to the kernel
 * @map_user		map memory to userspace
 *
 * allocate, phys, and map_user return 0 on success, -errno on error.
 * map_dma and map_kernel return pointer on success, ERR_PTR on
 * error. @free will be called with ION_PRIV_FLAG_SHRINKER_FREE set in
 * the buffer's private_flags when called from a shrinker. In that
 * case, the pages being free'd must be truly free'd back to the
 * system, not put in a page pool or otherwise cached.
 */
struct ion_heap_ops {
	int (*allocate)(struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long align, unsigned long flags);
	void (*free)(struct ion_buffer *buffer);
	void * (*map_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	void (*unmap_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);
	int (*map_user)(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma);
	int (*shrink)(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan);
};

/* [INTERNAL USE ONLY] threshold value for whole cache flush */
#define ION_FLUSH_ALL_HIGHLIMIT (SZ_8M + SZ_2M)

/**
 * heap flags - flags between the heaps and core ion code
 */
#define ION_HEAP_FLAG_DEFER_FREE (1 << 0)

/**
 * private flags - flags internal to ion
 */
/*
 * Buffer is being freed from a shrinker function. Skip any possible
 * heap-specific caching mechanism (e.g. page pools). Guarantees that
 * any buffer storage that came from the system allocator will be
 * returned to the system allocator.
 */
#define ION_PRIV_FLAG_SHRINKER_FREE (1 << 0)

/*
 * Following private flags are used for deferred init to boost up
 * allocation performance.
 * READY_TO_USE: set when buffer is initialized
 */
#define ION_PRIV_FLAG_READY_TO_USE (1 << 15)

/*
 * Following private flags are used for cache maintainence between
 * non sharable device and sharable device with cpu access.
 */
#define ION_PRIV_FLAG_NEED_TO_FLUSH (1 << 1)

/**
 * struct ion_heap - represents a heap in the system
 * @node:		rb node to put the heap on the device's tree of heaps
 * @dev:		back pointer to the ion_device
 * @type:		type of heap
 * @ops:		ops struct as above
 * @flags:		flags
 * @id:			id of heap, also indicates priority of this heap when
 *			allocating.  These are specified by platform data and
 *			MUST be unique
 * @name:		used for debugging
 * @shrinker:		a shrinker for the heap
 * @free_list:		free list head if deferred free is used
 * @free_list_size	size of the deferred free list in bytes
 * @lock:		protects the free list
 * @waitqueue:		queue to wait on from deferred free thread
 * @task:		task struct of deferred free thread
 * @debug_show:		called when heap debug file is read to add any
 *			heap specific debug info to output
 *
 * Represents a pool of memory from which buffers can be made.  In some
 * systems the only heap is regular system memory allocated via vmalloc.
 * On others, some blocks might require large physically contiguous buffers
 * that are allocated from a specially reserved heap.
 */
struct ion_heap {
	struct plist_node node;
	struct ion_device *dev;
	enum ion_heap_type type;
	struct ion_heap_ops *ops;
	unsigned long flags;
	unsigned int id;
	const char *name;
	struct shrinker shrinker;
	struct list_head free_list;
	size_t free_list_size;
	spinlock_t free_lock;
	wait_queue_head_t waitqueue;
	struct task_struct *task;

	int (*debug_show)(struct ion_heap *heap, struct seq_file *, void *);
	atomic_long_t total_allocated;
	atomic_long_t total_allocated_peak;
	atomic_long_t total_handles;
};

/**
 * ion_buffer_sync_force - check if ION_FLAG_SYNC_FORCE is set
 * @buffer:		buffer
 *
 * indicates whether this ion buffer should be cache clean after allocation
 */
static inline bool ion_buffer_sync_force(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_SYNC_FORCE);
}

/**
 * ion_buffer_cached - this ion buffer is cached
 * @buffer:		buffer
 *
 * indicates whether this ion buffer is cached
 */
static inline bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_CACHED);
}

/**
 * ion_buffer_fault_user_mappings - fault in user mappings of this buffer
 * @buffer:		buffer
 *
 * indicates whether userspace mappings of this buffer will be faulted
 * in, this can affect how buffers are allocated from the heap.
 */
static inline bool ion_buffer_fault_user_mappings(struct ion_buffer *buffer)
{
	return (buffer->flags & ION_FLAG_CACHED) &&
		!(buffer->flags & ION_FLAG_CACHED_NEEDS_SYNC);
}

#define ION_FLAGS_CACHED_SYNC_FORCE \
	(ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_SYNC_FORCE)

static inline bool ion_buffer_cache_clean_on_alloc(struct ion_buffer *buffer)
{
	return (buffer->flags & ION_FLAGS_CACHED_SYNC_FORCE) ==
				(ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC);
}

static inline bool ion_buffer_need_flush_all(struct ion_buffer *buffer)
{
	return buffer->size >= ION_FLUSH_ALL_HIGHLIMIT;
}

/**
 * ion_device_create - allocates and returns an ion device
 * @custom_ioctl:	arch specific ioctl function if applicable
 *
 * returns a valid device or -PTR_ERR
 */
struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg));

/**
 * ion_device_destroy - free and device and it's resource
 * @dev:		the device
 */
void ion_device_destroy(struct ion_device *dev);

/**
 * ion_device_add_heap - adds a heap to the ion device
 * @dev:		the device
 * @heap:		the heap to add
 */
void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap);

/**
 * some helpers for common operations on buffers using the sg_table
 * and vaddr fields
 */
void *ion_heap_map_kernel(struct ion_heap *, struct ion_buffer *);
void ion_heap_unmap_kernel(struct ion_heap *, struct ion_buffer *);
int ion_heap_map_user(struct ion_heap *, struct ion_buffer *,
			struct vm_area_struct *);
int ion_heap_buffer_zero(struct ion_buffer *buffer);
int ion_heap_pages_zero(struct page *page, size_t size, pgprot_t pgprot);

/**
 * ion_heap_init_shrinker
 * @heap:		the heap
 *
 * If a heap sets the ION_HEAP_FLAG_DEFER_FREE flag or defines the shrink op
 * this function will be called to setup a shrinker to shrink the freelists
 * and call the heap's shrink op.
 */
void ion_heap_init_shrinker(struct ion_heap *heap);

/**
 * ion_heap_init_deferred_free -- initialize deferred free functionality
 * @heap:		the heap
 *
 * If a heap sets the ION_HEAP_FLAG_DEFER_FREE flag this function will
 * be called to setup deferred frees. Calls to free the buffer will
 * return immediately and the actual free will occur some time later
 */
int ion_heap_init_deferred_free(struct ion_heap *heap);

/**
 * ion_heap_freelist_add - add a buffer to the deferred free list
 * @heap:		the heap
 * @buffer:		the buffer
 *
 * Adds an item to the deferred freelist.
 */
void ion_heap_freelist_add(struct ion_heap *heap, struct ion_buffer *buffer);

/**
 * ion_heap_freelist_drain - drain the deferred free list
 * @heap:		the heap
 * @size:		amount of memory to drain in bytes
 *
 * Drains the indicated amount of memory from the deferred freelist immediately.
 * Returns the total amount freed.  The total freed may be higher depending
 * on the size of the items in the list, or lower if there is insufficient
 * total memory on the freelist.
 */
size_t ion_heap_freelist_drain(struct ion_heap *heap, size_t size);

/**
 * ion_heap_freelist_shrink - drain the deferred free
 *				list, skipping any heap-specific
 *				pooling or caching mechanisms
 *
 * @heap:		the heap
 * @size:		amount of memory to drain in bytes
 *
 * Drains the indicated amount of memory from the deferred freelist immediately.
 * Returns the total amount freed.  The total freed may be higher depending
 * on the size of the items in the list, or lower if there is insufficient
 * total memory on the freelist.
 *
 * Unlike with @ion_heap_freelist_drain, don't put any pages back into
 * page pools or otherwise cache the pages. Everything must be
 * genuinely free'd back to the system. If you're free'ing from a
 * shrinker you probably want to use this. Note that this relies on
 * the heap.ops.free callback honoring the ION_PRIV_FLAG_SHRINKER_FREE
 * flag.
 */
size_t ion_heap_freelist_shrink(struct ion_heap *heap,
					size_t size);

/**
 * ion_heap_freelist_size - returns the size of the freelist in bytes
 * @heap:		the heap
 */
size_t ion_heap_freelist_size(struct ion_heap *heap);


/**
 * functions for creating and destroying the built in ion heaps.
 * architectures can add their own custom architecture specific
 * heaps as appropriate.
 */

struct ion_heap *ion_heap_create(struct ion_platform_heap *);
void ion_heap_destroy(struct ion_heap *);
struct ion_heap *ion_system_heap_create(struct ion_platform_heap *);
void ion_system_heap_destroy(struct ion_heap *);

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *);
void ion_system_contig_heap_destroy(struct ion_heap *);

#ifdef CONFIG_ION_RBIN_HEAP
struct ion_heap *ion_rbin_heap_create(struct ion_platform_heap *);
void ion_rbin_heap_destroy(struct ion_heap *);
#endif

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *);
void ion_carveout_heap_destroy(struct ion_heap *);

void ion_debug_heap_usage_show(struct ion_heap *heap);
struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *);
void ion_chunk_heap_destroy(struct ion_heap *);
struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *);
void ion_cma_heap_destroy(struct ion_heap *);

struct ion_heap *ion_hpa_heap_create(struct ion_platform_heap *);
void ion_hpa_heap_destroy(struct ion_heap *);

/**
 * functions for creating and destroying a heap pool -- allows you
 * to keep a pool of pre allocated memory to use from your heap.  Keeping
 * a pool of memory that is ready for dma, ie any cached mapping have been
 * invalidated from the cache, provides a significant performance benefit on
 * many systems
 */

/**
 * struct ion_page_pool - pagepool struct
 * @high_count:		number of highmem items in the pool
 * @low_count:		number of lowmem items in the pool
 * @high_items:		list of highmem items
 * @low_items:		list of lowmem items
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		plist node for list of pools
 * @cached:		it's cached pool or not
 *
 * Allows you to keep a pool of pre allocated pages to use from your heap.
 * Keeping a pool of pages that is ready for dma, ie any cached mapping have
 * been invalidated from the cache, provides a significant performance benefit
 * on many systems
 */
struct ion_page_pool {
	int high_count;
	int low_count;
	bool cached;
	struct list_head high_items;
	struct list_head low_items;
	spinlock_t lock;
	gfp_t gfp_mask;
	unsigned int order;
	struct plist_node list;
};

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
					   bool cached);
int ion_page_pool_total(struct ion_page_pool *pool, bool high);

void ion_page_pool_destroy(struct ion_page_pool *);
struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high);

#define PGMASK_FROM_BUDDY		BIT(PG_owner_priv_1)
#define ION_PAGE_FROM_BUDDY(page)    (((page)->flags & PGMASK_FROM_BUDDY) != 0)
#define ION_SET_PAGE_FROM_BUDDY(page)	((page)->flags |= PGMASK_FROM_BUDDY)
#define ION_CLEAR_PAGE_FROM_BUDDY(page)	((page)->flags &= ~PGMASK_FROM_BUDDY)

void *ion_page_pool_only_alloc(struct ion_page_pool *a);
struct page *ion_page_pool_alloc(struct ion_page_pool *pool, bool zeroed);
void ion_page_pool_free(struct ion_page_pool *, struct page *);

/** ion_page_pool_shrink - shrinks the size of the memory cached in the pool
 * @pool:		the pool
 * @gfp_mask:		the memory type to reclaim
 * @nr_to_scan:		number of items to shrink in pages
 *
 * returns the number of items freed in pages
 */
int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			  int nr_to_scan);

/**
 * ion_pages_sync_for_device - cache flush pages for use with the specified
 *                             device
 * @dev:		the device the pages will be used with
 * @page:		the first page to be flushed
 * @size:		size in bytes of region to be flushed
 * @dir:		direction of dma transfer
 */
void ion_pages_sync_for_device(struct device *dev, struct page *page,
		size_t size, enum dma_data_direction dir);

long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

int ion_sync_for_device(struct ion_client *client, int fd);

struct ion_handle *ion_handle_get_by_id_nolock(struct ion_client *client,
						int id);

bool ion_handle_validate(struct ion_client *client,
			 struct ion_handle *handle);

void ion_free_nolock(struct ion_client *client, struct ion_handle *handle);

int ion_handle_put_nolock(struct ion_handle *handle);

struct ion_handle *ion_handle_get_by_id(struct ion_client *client,
						int id);

int ion_handle_put(struct ion_client *client, struct ion_handle *handle);

int ion_query_heaps(struct ion_client *client, struct ion_heap_query *query);

struct ion_handle *__ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags, bool grab_handle);

int ion_share_dma_buf_fd_nolock(struct ion_client *client,
				struct ion_handle *handle);

#endif /* _ION_PRIV_H */
