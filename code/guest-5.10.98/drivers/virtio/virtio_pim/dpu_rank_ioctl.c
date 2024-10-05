/*
This file implements the file operation that does unlocked_ioctl.
when the sdk writes in file /dev/dpu to call ioctl the functions 
in this file will be called
*/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/ktime.h>
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/string.h>
#include "dpu.h"
#include "dpu_rank.h"
#include "dpu_region_address_translation.h"
#include "dpu_rank_ioctl.h"
#include "dpu_region_constants.h"
#include "dpu_utils.h"
#include "dpu_batch.h"
#include "dpu_prefetch.h"

/*
This function returns the first page of a specific dpu
*/
static struct page **get_page_array(struct dpu_rank_t *rank, int dpu_idx)
{
	uint32_t mram_size, nb_page_in_array;

	mram_size = rank->region->addr_translate.desc.memories.mram_size;
	nb_page_in_array = (mram_size / PAGE_SIZE);

	return &rank->xfer_dpu_page_array[dpu_idx * nb_page_in_array + dpu_idx];
}

/* Returns pages that must be put and free by calling function,
 * note that in case of success, the caller must release mmap_lock. */
/*
This function is responsible for preparing memory pages for a data transfer involving a DPU. It does the following:
Calculates the number of memory pages required for the transfer.
Allocates a structure (xfer_page) to keep track of these pages.
Uses get_user_pages to pin the user pages in memory. This is necessary for ensuring that the pages stay in memory during the data transfer.
Flushes the data cache for each page to maintain consistency.
Handles errors in pinning pages and logs them.
The function returns the number of pages pinned, or an error if the pinning fails.
*/
static int pin_pages_for_xfer(struct device *dev, struct dpu_rank_t *rank,
			      struct dpu_transfer_mram *xfer,
			      unsigned int gup_flags, int dpu_idx)
{
	struct xfer_page *xferp;
	unsigned long nb_pages, nb_pages_expected;
	uint32_t off_page;
	int i;
	uint8_t *ptr_user =
		xfer->ptr[dpu_idx]; /* very important to keep this address,
					* since it will get overriden by
					* get_user_pages
					*/

	/* Allocation from userspace may not be aligned to
	 * page size, compute the offset of the base pointer
	 * to the previous page boundary.
	 */
	off_page = ((unsigned long)ptr_user & (PAGE_SIZE - 1));

	nb_pages_expected = ((xfer->size + off_page) / PAGE_SIZE);
	nb_pages_expected += (((xfer->size + off_page) % PAGE_SIZE) ? 1 : 0);
	xferp = &rank->xfer_pg[dpu_idx];
	memset(xferp, 0, sizeof(struct xfer_page));

	xferp->pages = get_page_array(rank, dpu_idx);
	xferp->off_first_page = off_page;
	xferp->nb_pages = nb_pages_expected;

	xfer->ptr[dpu_idx] = xferp;

	/* No page to pin or flush, bail early */
	if (nb_pages_expected == 0)
		return 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	/* Note: If needed, PageTransHuge returns true in case of a huge page */
	nb_pages = get_user_pages((unsigned long)ptr_user, xferp->nb_pages,
				  gup_flags, xferp->pages, NULL);
#else
	nb_pages = get_user_pages(current, current->mm, (unsigned long)ptr_user,
				  xferp->nb_pages, gup_flags, 0, xferp->pages,
				  NULL);
#endif
	if (nb_pages <= 0 || nb_pages != nb_pages_expected) {
		dev_err(dev, "cannot pin pages: nb_pages %ld/expected %ld\n",
			nb_pages, nb_pages_expected);
		return -EFAULT;
	}

	for (i = 0; i < nb_pages; ++i)
		flush_dcache_page(xferp->pages[i]);

	return nb_pages;
}


/* Careful to release mmap_lock ! */
/*
This function extends pin_pages_for_xfer to handle a matrix of DPUs. 
It iterates over each DPU in a rank and: Pins pages for each DPU's transfer.
In case of an error during pinning, it releases the pages that were pinned for previous DPUs to avoid memory leaks.
It acquires and releases a lock (mmap_lock or mmap_sem, depending on the Linux kernel version) to ensure thread safety during the operation.
*/
static int pin_pages_for_xfer_matrix(struct device *dev,
				     struct dpu_rank_t *rank,
				     struct dpu_transfer_mram *xfer_matrix,
				     unsigned int gup_flags)
{
	struct dpu_region_address_translation *tr;
	uint8_t ci_id, dpu_id, nb_cis, nb_dpus_per_ci;
	int idx;
	int ret;

	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	down_read(&current->mm->mmap_sem);
#else
	down_read(&current->mm->mmap_lock);
#endif

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		/* Here we work 'in-place' in xfer_matrix by replacing pointers
		 * to userspace buffers in struct dpu_transfer_mram * by newly
		 * allocated struct page ** representing the userspace buffer.
		 */
		if (!xfer_matrix->ptr[idx])
			continue;

		ret = pin_pages_for_xfer(dev, rank, xfer_matrix, gup_flags,
					 idx);
		if (ret < 0) {
			int i, j;

			for (i = idx - 1; i >= 0; --i) {
				if (xfer_matrix->ptr[i]) {
					struct xfer_page *xferp;

					xferp = xfer_matrix->ptr[i];

					for (j = 0; j < xferp->nb_pages; ++j)
						put_page(xferp->pages[j]);
				}
			}
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
			up_read(&current->mm->mmap_sem);
#else
			up_read(&current->mm->mmap_lock);
#endif
			return ret;
		}
	}

	return 0;
}

int nb_ci_requests = 0;

static int dpu_rank_write_to_rank(struct dpu_rank_t *rank, unsigned long arg);
static int dpu_rank_read_from_rank(struct dpu_rank_t *rank, unsigned long arg);
static int dpu_rank_commit_commands(struct dpu_rank_t *rank, unsigned long arg);
static int dpu_rank_update_commands(struct dpu_rank_t *rank, unsigned long arg);
static int dpu_rank_debug_mode(struct dpu_rank_t *rank, unsigned long arg);

long dpu_rank_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct dpu_rank_t *rank = filp->private_data;
	int result = -EINVAL;
	ktime_t start;
	if (!rank) return 0;
	start = ktime_get();
	dev_dbg(&rank->dev, "ioctl rank_id %u\n", rank->id);
	switch (cmd) {
	case DPU_RANK_IOCTL_WRITE_TO_RANK:
		dpu_batch_flush_CI(rank);
		dpu_prefetch_reset_cache(rank);
		//printk(KERN_CRIT "#### Kernel: Write to rank on rank %d\n", rank->id);
		result = dpu_rank_write_to_rank(rank, arg);
		//start = ktime_sub(ktime_get(), start);
		break;

	case DPU_RANK_IOCTL_READ_FROM_RANK:
		dpu_batch_flush_CI(rank);
		dpu_batch_flush_rank(rank);
		result = dpu_rank_read_from_rank(rank, arg);
		//start = ktime_sub(ktime_get(), start);
		//printk(KERN_CRIT "#### Kernel: Read from rank time %llu ms\n", ktime_to_ms(start));
		break;

	case DPU_RANK_IOCTL_COMMIT_COMMANDS:
		dpu_batch_flush_rank(rank);
		dpu_prefetch_reset_cache(rank);
		result = dpu_rank_commit_commands(rank, arg);
		break;

	case DPU_RANK_IOCTL_UPDATE_COMMANDS:
		dpu_batch_flush_CI(rank);
		dpu_batch_flush_rank(rank);
		dpu_prefetch_reset_cache(rank);
		result = dpu_rank_update_commands(rank, arg);
		break;

	case DPU_RANK_IOCTL_DEBUG_MODE:
		result = dpu_rank_debug_mode(rank, arg);
		break;

	default:
		break;
	}
	return result;
}

/* Retrieve matrix transfer from userspace */
static int dpu_rank_get_user_xfer_matrix(struct dpu_transfer_mram *xfer_matrix,
					 unsigned long ptr)
{
	if (copy_from_user(xfer_matrix, (void *)ptr, sizeof(*xfer_matrix)))
		return -EFAULT;

	return 0;
}


/*
* @brief: 	This function handles the sdk request to write to the dpu rank
* @param:  	rank is the dpu_rank we want to write to
			arg is an address which provides information 
			to get the xfer_matrix from the user space

This function first retrieve the xfer_matrix from the user space
Then it calls the function from dpu_page_management to pin pages for transfer
After this, it calls the wrtie_to_rank function which redirect to the virtio_translate
which generate requests to the hypervisor
When writing is finished, it frees the pages
*/
static int dpu_rank_write_to_rank(struct dpu_rank_t *rank, unsigned long arg){
    struct device *dev = &rank->dev;
	struct dpu_region_address_translation *tr;
	struct dpu_transfer_mram xfer_matrix;
	int i, ret = 0;
	uint8_t ci_id, dpu_id, nb_cis, nb_dpus_per_ci;
	int idx;

	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	ret = dpu_rank_get_user_xfer_matrix(&xfer_matrix, arg);
	if (ret)
		return ret;

	/* Pin pages of all the buffers in the transfer matrix, and start
	 * the transfer: from here we are committed to release mmap_lock.
	 */
	ret = pin_pages_for_xfer_matrix(dev, rank, &xfer_matrix, 0);
	if (ret)
		return ret;

	/* Launch the transfer */
	dpu_batch_write_to_rank(rank, &xfer_matrix);
	//tr->write_to_rank(tr, rank->region->base, rank->channel_id, &xfer_matrix);

	/* Free pages */
	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		if (xfer_matrix.ptr[idx]) {
			struct xfer_page *xferp;

			xferp = xfer_matrix.ptr[idx];

			for (i = 0; i < xferp->nb_pages; ++i)
				put_page(xferp->pages[i]);
		}
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	up_read(&current->mm->mmap_sem);
#else
	up_read(&current->mm->mmap_lock);
#endif
	return ret;
}


/*
* @brief: 	This function handles the sdk request to read to the dpu rank
* @param:  	rank is the dpu_rank we want to write to
			arg is an address which provides information 
			to get the xfer_matrix from the user space

This function's workflow is exactly the same as the last function dpu_rank_write_to_rank
But this function does read instead of write
*/
static int dpu_rank_read_from_rank(struct dpu_rank_t *rank, unsigned long arg){
    struct device *dev = &rank->dev;
	struct dpu_region_address_translation *tr;
	struct dpu_transfer_mram xfer_matrix;
	int i, ret = 0;
	uint8_t ci_id, dpu_id, nb_cis, nb_dpus_per_ci;
	int idx;

	//ktime_t start;

	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;
	ret = dpu_rank_get_user_xfer_matrix(&xfer_matrix, arg);

	if (ret)
		return ret;

	/* Pin pages of all the buffers in the transfer matrix, and start
	 * the transfer. Check if the buffer is writable and do not forget
	 * to fault in pages...
	 */
	//start = ktime_get();
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	ret = pin_pages_for_xfer_matrix(dev, rank, &xfer_matrix,
					FOLL_WRITE | FOLL_POPULATE);
#else
	ret = pin_pages_for_xfer_matrix(dev, rank, &xfer_matrix, FOLL_WRITE);
#endif
	if (ret)
		return ret;
	
	//start = ktime_sub(ktime_get(), start);
	//printk(KERN_CRIT "#### Kernel: Page time %llu ms\n", ktime_to_ms(start));

	if(!dpu_prefetch_search_cache(rank, &xfer_matrix)){
		/* Launch the transfer */
		tr->read_from_rank(tr, rank->region->base, rank->channel_id, &xfer_matrix);
	}

	for_each_dpu_in_rank(idx, ci_id, dpu_id, nb_cis, nb_dpus_per_ci)
	{
		if (xfer_matrix.ptr[idx]) {
			struct xfer_page *xferp;
			xferp = xfer_matrix.ptr[idx];
			for (i = 0; i < xferp->nb_pages; ++i) {
				put_page(xferp->pages[i]);
			}
		}
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	up_read(&current->mm->mmap_sem);
#else
	up_read(&current->mm->mmap_lock);
#endif

	return ret;
}

/*
* @brief: 	This function handles the sdk request to write to control_interface (CI)
* @param:  	rank is the dpu_rank we want to write to
			arg is the address of the desired CI
This function first copy the control interface to the ci in data structure dpu_rank
Then it calls the function write_to_cis to send request to the hypervisor
*/
static int dpu_rank_commit_commands(struct dpu_rank_t *rank, unsigned long arg){
	struct dpu_region_address_translation *tr;
	uint32_t size_command;
	uint8_t nb_cis;
	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	size_command = sizeof(uint64_t) * nb_cis;
	memset(rank->control_interface, 0, size_command);
	if (copy_from_user(rank->control_interface, (uint8_t *)arg, size_command))
		return -EFAULT;
	dpu_batch_write_to_cis(rank);
	//tr->write_to_cis(tr, rank->region->base, rank->channel_id, rank->control_interface);
	return 0;
}

/*
* @brief: 	This function handles the sdk request to read from control_interface (CI)
* @param:  	rank is the dpu_rank we want to write to
			arg is the address of the desired CI
This function first calls the function read_from_cis to send request to the hypervisor
Then it copy the result control interface to the ci in data structure dpu_rank
*/
static int dpu_rank_update_commands(struct dpu_rank_t *rank, unsigned long arg){
	struct dpu_region_address_translation *tr;
	uint32_t size_command;
	uint8_t nb_cis;
	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	size_command = sizeof(uint64_t) * nb_cis;
	memset(rank->control_interface, 0, size_command);
	tr->read_from_cis(tr, rank->region->base, rank->channel_id, rank->control_interface);
	if (copy_to_user((uint8_t *)arg, rank->control_interface, size_command))
		return -EFAULT;	
	return 0;
}

static int dpu_rank_debug_mode(struct dpu_rank_t *rank, unsigned long arg){
	dpu_region_lock(rank->region);
	rank->debug_mode = arg;
	dpu_region_unlock(rank->region);
	return 0;
}