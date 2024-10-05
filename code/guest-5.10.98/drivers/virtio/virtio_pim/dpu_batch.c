#include <linux/printk.h>
#include <linux/virtio.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include "dpu_batch.h"
#include "dpu.h"
#include "dpu_types.h"
#include "dpu_region_address_translation.h"


#if DPU_BATCH_ENABLE
#if DPU_BATCH_CI_ENABLE


int dpu_batch_flush_CI(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
    if(rank->ci_batch_index == 0)
        return 0;
    //printk(KERN_CRIT "#### Start flushing CI, batched CI requests: %d\n", rank->ci_batch_index);
    tr = &rank->region->addr_translate;
    tr->write_to_cis_batch(tr, rank->region->base, rank->channel_id, rank->ci_batch);
    rank->ci_batch_index = 0;
    return 0;
}

int dpu_batch_write_to_cis(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
    uint32_t size_command;
    tr = &rank->region->addr_translate;
	size_command = sizeof(uint64_t) * DPU_MAX_NR_CIS;
    memcpy(rank->ci_batch + rank->ci_batch_index * DPU_MAX_NR_CIS, rank->control_interface, size_command);
    rank->ci_batch_index++;
    if(rank->ci_batch_index==CI_BATCH_SIZE){
        rank->ci_batch_index = 0;
        dpu_batch_flush_CI(rank);
    }
    return 0;
}
#else
int dpu_batch_flush_CI(struct dpu_rank_t *rank){
    return 0;
}

int dpu_batch_write_to_cis(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
    tr = &rank->region->addr_translate;
    tr->write_to_cis(tr, rank->region->base, rank->channel_id, rank->control_interface);
    return 0;
}
#endif


int dpu_batch_flush_rank(struct dpu_rank_t *rank){
    //transfer the rank->xfer_batch to firecracker
    //clean the rank->rank_pages
    struct dpu_region_address_translation *tr;
    if(rank->rank_batch_index == 0){
        rank->rank_page_index = 0;
        return 0;  
    }
    //printk(KERN_CRIT "#### Start flushing rank, batched rank requests: %d\n", rank->rank_batch_index);
    tr = &rank->region->addr_translate;
    tr->write_to_rank_batch(tr, rank->region->base, rank->channel_id);
    rank->rank_batch_index = 0;
    rank->rank_page_index = 0;
    return 0;
}

int dpu_batch_write_to_rank(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix){
    struct dpu_region_address_translation *tr;
    uint32_t xfer_nb_pages_per_dpu;
    uint32_t current_xfer_pages = 0;
    uint8_t nr_cis, nr_dpus_per_ci;
    uint32_t current_batch = rank->rank_batch_index;
    uint32_t start_offset;
    uint32_t i,j;
    struct xfer_page *xferp;

    //printk(KERN_CRIT "#### dpu_batch_write_to_rank\n");


    tr = &rank->region->addr_translate;
    nr_cis = tr->desc.topology.nr_of_control_interfaces;
	nr_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;
    //case 1: if xfer_matrix is already large enough, launch the transfer.
    xfer_nb_pages_per_dpu = ((xfer_matrix->size ) / PAGE_SIZE) + 1;
    if(xfer_nb_pages_per_dpu > DPU_BATCH_PAGE_THRESHOLD){
        //printk(KERN_CRIT "#### case 1: if xfer_matrix is already large enough, launch the transfer.\n");    
        goto direct_xfer;
    }
    
    //case 2: if pages xfer_matrix cannot fit in the remaining rank_pages
    for(i=0; i<nr_cis*nr_dpus_per_ci; i++){
		xferp = xfer_matrix->ptr[i];
        if(!xferp) continue; 
        current_xfer_pages+=xferp->nb_pages;
	}
    if(current_xfer_pages+rank->rank_page_index >= DPU_RANK_PAGES){
        //printk(KERN_CRIT "#### case 2: if current_xfer_pages: %d > rank_page_index: %d\n", current_xfer_pages, rank->rank_page_index);    
        goto direct_xfer;
    }
    
    /*case 3.1: pages can be fit in the remaining rank_pages, use the xfer_matrix to fill the rank->xfer_batch's field at rank->rank_batch_index as follows
    fill the offset_in_mram and size at rank_batch_index
    copy the pages in xfer_matrix to the pages in rank_pages from rank_page_index
    assign the rank_pages to the corresponding pages of xfer_page_batch in xfer_batch
    which is the from a certain offset: the sum of nb_pages of all previous batch
    assgn the nb_pages of each xferp to the current_batch-th nb_pages of the corresponding xfer_page_batch
    assign the off_first_page of each xferp to the current_batch-th off_first_page of the corresponding xfer_page_batch*/
    // Fill offset_in_mram and size at rank_batch_index for each DPU

    //printk(KERN_CRIT "#### case 3.1: pages can be fit in the remaining rank_pages. Batch the request\n"); 
    rank->xfer_batch.matrix_infos[current_batch].offset_in_mram = xfer_matrix->offset_in_mram;
    rank->xfer_batch.matrix_infos[current_batch].size = xfer_matrix->size;
    for (i = 0; i < nr_cis * nr_dpus_per_ci; i++) {
        xferp = xfer_matrix->ptr[i];
        if (!xferp) {
            rank->xfer_batch.xferpb[i].xferp_infos[current_batch].nb_pages =0;
            rank->xfer_batch.xferpb[i].xferp_infos[current_batch].off_first_page = 0;
            continue;
        }
        // Calculate the starting offset for this batch
        start_offset = 0;
        for (j = 0; j < current_batch; j++) {
            start_offset += rank->xfer_batch.xferpb[i].xferp_infos[j].nb_pages;
        }

        // Copy the content of pages to rank_pages and assign to xfer_page_batch
        for (j = 0; j < xferp->nb_pages; j++) {
            void *src_addr = kmap(xferp->pages[j]);
            void *dest_addr = kmap(rank->rank_pages[rank->rank_page_index]);
            memcpy(dest_addr, src_addr, PAGE_SIZE);
            kunmap(xferp->pages[j]);
            kunmap(rank->rank_pages[rank->rank_page_index]);
            rank->xfer_batch.xferpb[i].pages[start_offset + j] = rank->rank_pages[rank->rank_page_index];
            rank->rank_page_index++;
        }

        // Assign nb_pages and off_first_page
        rank->xfer_batch.xferpb[i].xferp_infos[current_batch].nb_pages = xferp->nb_pages;
        rank->xfer_batch.xferpb[i].xferp_infos[current_batch].off_first_page = xferp->off_first_page;
    }
    rank->rank_batch_index++;
    if(rank->rank_batch_index >= XFER_BATCH_SIZE){
        //printk(KERN_CRIT "#### case 3.2: number batched request equals XFER_BATCH_SIZE, launch xfer.\n"); 
        dpu_batch_flush_rank(rank);
        rank->rank_batch_index=0;
    }
    return 0;

direct_xfer:   
    dpu_batch_flush_rank(rank);
    tr->write_to_rank(tr, rank->region->base, rank->channel_id, xfer_matrix);
    return 0;
}

#else
int dpu_batch_flush_CI(struct dpu_rank_t *rank){
    return 0;
}

int dpu_batch_write_to_cis(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
    tr = &rank->region->addr_translate;
    tr->write_to_cis(tr, rank->region->base, rank->channel_id, rank->control_interface);
    return 0;
}

int dpu_batch_flush_rank(struct dpu_rank_t *rank){
    return 0;
}

int dpu_batch_write_to_rank(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix){
    struct dpu_region_address_translation *tr;
    tr = &rank->region->addr_translate;
    tr->write_to_rank(tr, rank->region->base, rank->channel_id, xfer_matrix);
    return 0;
}

#endif



