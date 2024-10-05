/*
This file contains functions that is about dpu_rank
It can be separated in two parts
1. Create the device and initialize a file at /dev/dpu_rank
	This file is used by the sdk to communicate with the driver.
	The sdk will write command to this file.
	This operation is done at boot time

2. Defines the operations about this file
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
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/virtio.h>
#include "dpu.h"
#include "dpu_rank.h"
#include "dpu_region_address_translation.h"
#include "dpu_rank_ioctl.h"
#include "dpu_region_constants.h"
#include "dpu_utils.h"
#include "dpu_batch.h"
#include "dpu_prefetch.h"

//int backend_is_owned;
LIST_HEAD(rank_list);
struct class *dpu_rank_class;
static int counter_rank = 0;

static DEFINE_MUTEX(rank_allocator_lock);

static void dpu_rank_allocator_lock(void)
{
	mutex_lock(&rank_allocator_lock);
}

static void dpu_rank_allocator_unlock(void)
{
	mutex_unlock(&rank_allocator_lock);
}

//Part 2: The file operations
static int dpu_rank_create_device(struct device *dev_parent,
				  struct dpu_region *region,
				  struct dpu_rank_t *rank, bool must_init_mram);

static int dpu_rank_open(struct inode *inode, struct file *filp);

static int dpu_rank_release(struct inode *inode, struct file *filp);

static int dpu_rank_mmap(struct file *filp, struct vm_area_struct *vma);


//This struct define the operation that can be done on file /dev/dpu_rank
//Note that the dpu_rank_ioctl is defined in file dpu_rank_ioctl
static struct file_operations dpu_rank_fops = 
{ 
	.owner = THIS_MODULE,
	.open = dpu_rank_open,
	.release = dpu_rank_release,
	.unlocked_ioctl = dpu_rank_ioctl,
	.mmap = dpu_rank_mmap 
};

uint32_t dpu_rank_get(struct dpu_rank_t *rank)
{
	struct dpu_region_address_translation *tr =
		&rank->region->addr_translate;

	dpu_region_lock(rank->region);

	if (rank->owner.is_owned && !rank->debug_mode) {
		dpu_region_unlock(rank->region);
		return DPU_ERR_DRIVER;
	}

	/* Do not init the rank when attached in debug mode */
	if (rank->owner.usage_count == 0) {
		uint8_t each_ci;
		
		dpu_rank_allocator_lock();
		if ((tr->init_rank) && (tr->init_rank(tr, rank->channel_id))) {
			dpu_rank_allocator_unlock();
			dpu_region_unlock(rank->region);
			pr_warn("Failed to allocate rank, error at initialization.\n");
			return DPU_ERR_DRIVER;
		}
		rank->owner.is_owned = 1;
		dpu_rank_allocator_unlock();

		/* Clear cached values */
		for (each_ci = 0; each_ci < DPU_MAX_NR_CIS; ++each_ci) {
			rank->runtime.control_interface.slice_info[each_ci]
				.structure_value = 0ULL;
			rank->runtime.control_interface.slice_info[each_ci]
				.slice_target.type = DPU_SLICE_TARGET_NONE;
			// TODO Quid of host_mux_mram_state ?
		}
	}
	rank->owner.usage_count++;

	dpu_region_unlock(rank->region);

	return DPU_OK;
}

void dpu_rank_put(struct dpu_rank_t *rank)
{
	struct dpu_region_address_translation *tr =
		&rank->region->addr_translate;

	dpu_region_lock(rank->region);

	rank->owner.usage_count--;
	if (rank->owner.usage_count == 0) {
		dpu_rank_allocator_lock();
		rank->owner.is_owned = 0;
		if (tr->destroy_rank)
			tr->destroy_rank(tr, rank->channel_id);
		
		dpu_rank_allocator_unlock();

		/*
         * Make sure we do not leave the region open whereas the rank
         * was freed.
         */
		rank->debug_mode = 0;
		rank->region->mode = DPU_REGION_MODE_SAFE;
	}

	dpu_region_unlock(rank->region);
}


static int dpu_rank_open(struct inode *inode, struct file *filp)
{
	struct dpu_rank_t *rank =
		container_of(inode->i_cdev, struct dpu_rank_t, cdev);

	dev_dbg(&rank->dev, "opened rank_id %u\n", rank->id);

	filp->private_data = rank;

	if (dpu_rank_get(rank) != DPU_OK)
		return -EINVAL;



	rank->region->addr_translate.realloc_config(&(rank->region->addr_translate)); 
	return 0;
}

static int dpu_rank_release(struct inode *inode, struct file *filp)
{
	struct dpu_rank_t *rank = filp->private_data;

	if (!rank)
		return 0;

	dev_dbg(&rank->dev, "closed rank_id %u\n", rank->id);

	dpu_rank_put(rank);

	rank->region->addr_translate.free_rank(&(rank->region->addr_translate)); 

	return 0;
}

static int dpu_rank_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct dpu_rank_t *rank = filp->private_data;
	struct dpu_region_address_translation *tr;
	int ret = 0;

	tr = &rank->region->addr_translate;

	dpu_region_lock(rank->region);

	switch (rank->region->mode) {
	case DPU_REGION_MODE_UNDEFINED:
		if ((tr->capabilities & CAP_HYBRID) == 0) {
			ret = -EINVAL;
			goto end;
		}

		rank->region->mode = DPU_REGION_MODE_HYBRID;

		break;
	case DPU_REGION_MODE_SAFE:
	case DPU_REGION_MODE_PERF:
		/* TODO: Can we return a value that is not correct
                         * regarding man mmap ?
                         */
		dev_err(&rank->dev, "device already open"
				    " in perf or safe mode\n");
		ret = -EPERM;
		goto end;
	case DPU_REGION_MODE_HYBRID:
		break;
	}

end:
	dpu_region_unlock(rank->region);

	return ret ? ret : tr->mmap_hybrid(tr, filp, vma);
}

int init_dpu_rank_pages(struct dpu_rank_t *rank) {
    int i, j;
	rank->rank_pages = kmalloc(sizeof(struct page *) * DPU_RANK_PAGES, GFP_KERNEL);
	for (i = 0; i < DPU_RANK_PAGES; i++) {
		rank->rank_pages[i] = alloc_pages(GFP_KERNEL, 0);
		if (!rank->rank_pages[i]) {
			while (i--) {
				__free_pages(rank->rank_pages[i], 0);
			}
			kfree(rank->rank_pages);
			return -ENOMEM;
		}
	}
    for (i = 0; i < DPU_MAX_NR_DPUS; ++i) {
        // Allocate memory for the pages array in each xfer_page_batch
        rank->xfer_batch.xferpb[i].pages = kmalloc(sizeof(struct page *) * DPU_RANK_PAGES, GFP_KERNEL);
        if (!rank->xfer_batch.xferpb[i].pages) {
            // Handle allocation failure
            printk(KERN_ERR "Failed to allocate pages array\n");
            // Free previously allocated arrays
            for (j = 0; j < i; j++) {
                kfree(rank->xfer_batch.xferpb[j].pages);
            }
            return -ENOMEM;
        }
        for (j = 0; j < DPU_RANK_PAGES; ++j) {
            rank->xfer_batch.xferpb[i].pages[j] = NULL;
        }
    }
	return 0;
}


int init_dpu_prefetch_cache(struct dpu_rank_t *rank){
    int i, j;
    struct xfer_page *xferp;
    rank->xfer_cache.offset_in_mram = 0;
    rank->xfer_cache.size = 0;

    for (i = 0; i < DPU_MAX_NR_DPUS; i++) {
        xferp = &rank->xfer_cache.xferp[i]; // Get a pointer to the xfer_page
        xferp->nb_pages = 0;
        xferp->off_first_page = 0;
        xferp->pages = kmalloc(sizeof(struct page *) * DPU_PREFETCH_CACHE_PAGES, GFP_KERNEL);
        if (!xferp->pages) {
            // Handle allocation failure for xferp->pages
            return -ENOMEM;
        }
        for (j = 0; j < DPU_PREFETCH_CACHE_PAGES; j++) {
            xferp->pages[j] = alloc_pages(GFP_KERNEL, 0);
            if (!xferp->pages[j]) {
                while (j--) {
                    __free_pages(xferp->pages[j], 0);
                }
                kfree(xferp->pages);
                return -ENOMEM;
            }
        }
    }
    return 0;
}


//Part 1: The initialization

//Functions belows are part of the probe function
/*
* @brief: 
		This function is responsible to fill up the entries
		of data structure dpu_rank
* @param: the device, the region and a flag
*/
int dpu_rank_init_device(struct device *dev, struct dpu_region *region,
			 bool must_init_mram)
{
	struct dpu_region_address_translation *tr;
	struct dpu_rank_t *rank;
	int ret;
	uint8_t nr_cis, each_ci;
	uint8_t nr_dpus_per_ci, each_dpu;
	tr = &region->addr_translate;
	rank = &region->rank;
	rank->channel_id = 0xff;
	rank->rank_index = 0; //TODO: check how to do rank_index
	rank->ci_batch_index = 0;
	rank->rank_page_index = 0;
	rank->rank_batch_index = 0;
	if(DPU_BATCH_ENABLE){
		ret = init_dpu_rank_pages(rank);
		if (ret)
			goto free_dpus;
	}else{
		rank->rank_pages = NULL;
	}
	if(DPU_PREFETCH_ENABLE){
		ret = init_dpu_prefetch_cache(rank);
		if (ret)
			goto free_dpus;
	}

	/* Assume all DPUs are enabled */
	nr_cis = tr->desc.topology.nr_of_control_interfaces;
	nr_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;

	for (each_ci = 0; each_ci < nr_cis; ++each_ci) {
		struct dpu_configuration_slice_info_t *slice_info =
			&rank->runtime.control_interface.slice_info[each_ci];

		slice_info->enabled_dpus = (1 << nr_dpus_per_ci) - 1;
		slice_info->all_dpus_are_enabled = true;
	}

	rank->dpus = kzalloc(nr_dpus_per_ci * nr_cis * sizeof(*(rank->dpus)),
			     GFP_KERNEL);
	if (!rank->dpus)
		return -ENOMEM;

	for (each_ci = 0; each_ci < nr_cis; ++each_ci) {
		for (each_dpu = 0; each_dpu < nr_dpus_per_ci; ++each_dpu) {
			uint8_t dpu_index =
				(nr_dpus_per_ci * each_ci) + each_dpu;
			struct dpu_t *dpu = rank->dpus + dpu_index;
			dpu->rank = rank;
			dpu->slice_id = each_ci;
			dpu->dpu_id = each_dpu;
			dpu->enabled = true;
		}
	}

	ret = dpu_rank_create_device(dev, region, rank, must_init_mram);
	if (ret)
		goto free_dpus;
		
	list_add_tail(&rank->list, &rank_list);
	return 0;

free_dpus:
	kfree(rank->dpus);
	return ret;
}


//This is part of the function above
//The calculations to fill up the data structure
static int dpu_rank_create_device(struct device *dev_parent,
				  struct dpu_region *region,
				  struct dpu_rank_t *rank, bool must_init_mram)
{
	int ret;
	uint32_t mram_size, dpu_size_page_array;
	uint8_t nb_cis, nb_dpus_per_ci;
	ret = alloc_chrdev_region(&rank->dev.devt, 0, 1, DPU_RANK_NAME);
	if (ret)
		return ret;

	cdev_init(&rank->cdev, &dpu_rank_fops);
	rank->cdev.owner = THIS_MODULE;

	device_initialize(&rank->dev);

	nb_cis = region->addr_translate.desc.topology.nr_of_control_interfaces;
	nb_dpus_per_ci = region->addr_translate.desc.topology
				 .nr_of_dpus_per_control_interface;
	mram_size = region->addr_translate.desc.memories.mram_size;
	dpu_size_page_array =
		((mram_size / PAGE_SIZE) + 1) * sizeof(struct page *);
	rank->xfer_dpu_page_array =
		vmalloc(nb_cis * nb_dpus_per_ci * dpu_size_page_array);
	if (!rank->xfer_dpu_page_array) {
		goto free_device_ref_and_chrdev;
	}
	
	rank->owner.is_owned = 0;
	rank->owner.usage_count = 0;

	rank->region = region;

	rank->dev.class = dpu_rank_class;
	
	rank->dev.parent = dev_parent;
	
	dev_set_drvdata(&rank->dev, rank);
	
	dev_set_name(&rank->dev, DPU_RANK_PATH, counter_rank++);


	ret = cdev_add(&rank->cdev, rank->dev.devt, 1);
	if (ret)
		goto free_dpu_page_array;
	ret = device_add(&rank->dev);
	if (ret)
		goto free_cdev;
	return 0; 
//TODO see if we need to uncomment
free_cdev:
	cdev_del(&rank->cdev);
free_dpu_page_array:
	vfree(rank->xfer_dpu_page_array);
free_device_ref_and_chrdev:
	put_device(&rank->dev);
	unregister_chrdev_region(rank->dev.devt, 1);
	return ret;
}


void dpu_rank_release_device(struct dpu_region *region)
{
	struct dpu_rank_t *rank = &region->rank;

	pr_info("dpu_rank: releasing rank\n");

	list_del(&rank->list);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	cdev_device_del(&rank->cdev, &rank->dev);
#else
	device_del(&rank->dev);
	cdev_del(&rank->cdev);
#endif
	vfree(rank->xfer_dpu_page_array);
	put_device(&rank->dev);
	unregister_chrdev_region(rank->dev.devt, 1);
	kfree(rank->dpus);
}

