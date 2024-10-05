#include <linux/printk.h>
#include <linux/virtio.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/uaccess.h> // For access_ok, VERIFY_READ
#include "dpu_prefetch.h"
#include "dpu.h"
#include "dpu_types.h"
#include "dpu_region_address_translation.h"


#if DPU_PREFETCH_ENABLE

bool condition(struct dpu_transfer_mram *xfer_matrix){
    if(xfer_matrix->ptr[8]!=NULL) return true;
    return false;
}

uint32_t count_xfer_dpu(struct dpu_transfer_mram *xfer_matrix){
    uint32_t count=0;
    uint32_t dpu_index;
    for(dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++){
        if(xfer_matrix->ptr[dpu_index]!=NULL) count++;
    }
    return count;
}

void print_xfer_cache(struct dpu_rank_t *rank) {
    struct xfer_page *xferp;
    void *page_addr;
    uint8_t *data;
    int i, dpu_index, j;

    printk(KERN_CRIT "xfer_cache: offset_in_mram = %u, size = %u\n", rank->xfer_cache.offset_in_mram, rank->xfer_cache.size);

    // Find the first xferp with nb_pages not zero
    for (dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++) {
        xferp = &rank->xfer_cache.xferp[dpu_index];
        if (xferp->nb_pages > 0) {
            printk(KERN_CRIT "First non-empty xferp found at DPU index %d:\n", dpu_index);
            for (i = 0; i < DPU_PREFETCH_CACHE_PAGES && i < xferp->nb_pages; i++) {
                page_addr = kmap(xferp->pages[i]);
                if (page_addr) {
                    data = (uint8_t *)page_addr;

                    printk(KERN_CRIT "Page %d first 5 bytes: ", i);
                    for (j = 0; j < 5; j++) {
                        printk(KERN_CONT "%02x ", data[j]);
                    }
                    printk(KERN_CRIT "\n");
                    kunmap(xferp->pages[i]);
                } else {
                    printk(KERN_CRIT "Failed to map page %d\n", i);
                }
            }
            break; // Stop after printing the first non-empty xferp
        }
    }

    if (dpu_index == DPU_MAX_NR_DPUS) {
        printk(KERN_CRIT "No non-empty xferp found in the cache.\n");
    }
}


void construct_prefetch_xfer_matrix(struct dpu_rank_t *rank, struct dpu_transfer_mram *old_xfer_matrix, struct dpu_transfer_mram *new_xfer_matrix) {
    int dpu_index = 0;
    new_xfer_matrix->offset_in_mram = old_xfer_matrix->offset_in_mram;
    new_xfer_matrix->size = DPU_PREFETCH_CACHE_PAGES * PAGE_SIZE;   
    for (dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++) {
        if (old_xfer_matrix->ptr[dpu_index] != NULL) {
            new_xfer_matrix->ptr[dpu_index] = &rank->xfer_cache.xferp[dpu_index];
            rank->xfer_cache.xferp[dpu_index].nb_pages = DPU_PREFETCH_CACHE_PAGES;
        } else {
            new_xfer_matrix->ptr[dpu_index] = NULL;
            rank->xfer_cache.xferp[dpu_index].nb_pages = 0;
        }
    }
}


void populate_xfer_matrix_from_cache(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix) {
    struct xfer_page *cached_page, *user_xfer_page;
    unsigned long copied, to_copy;
    unsigned long cache_offset, user_page_offset, cache_page_index;
    void *cached_page_addr, *user_page_addr;
    int user_page_index;
    int dpu_index = 0;

    //print_xfer_cache(rank);
    //if(condition(xfer_matrix)) printk(KERN_CRIT "Populating xfer_matrix from cache: MRAM offset = %u, size = %u\n", xfer_matrix->offset_in_mram, xfer_matrix->size);

    cache_offset = xfer_matrix->offset_in_mram - rank->xfer_cache.offset_in_mram;

    for (dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++) {
        user_xfer_page = (struct xfer_page *)xfer_matrix->ptr[dpu_index];
        if (user_xfer_page != NULL) {
            cached_page = &rank->xfer_cache.xferp[dpu_index];
            copied = 0;
            user_page_index = 0;  // Initialize user page index

            while (copied < xfer_matrix->size) {
                cache_page_index = (cache_offset + copied) / PAGE_SIZE;
                user_page_offset = (copied == 0) ? user_xfer_page->off_first_page : 0;
                to_copy = min(PAGE_SIZE - user_page_offset, xfer_matrix->size - copied);

                cached_page_addr = kmap(cached_page->pages[cache_page_index]);
                user_page_addr = kmap(user_xfer_page->pages[user_page_index]);
                memcpy(user_page_addr + user_page_offset, cached_page_addr + ((cache_offset + copied) % PAGE_SIZE), to_copy);
                kunmap(cached_page->pages[cache_page_index]);
                kunmap(user_xfer_page->pages[user_page_index]);
                copied += to_copy;
                if (copied >= PAGE_SIZE - user_xfer_page->off_first_page) {
                    user_page_index++;  // Move to the next page after filling the first one
                }
                //if(condition(xfer_matrix)) printk(KERN_CRIT "Copying %lu bytes from cache page %lu to user page %d (offset %lu).\n", to_copy, cache_page_index, user_page_index, user_page_offset);
            }
        }
    }
}


bool dpu_prefetch_cache_hit(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix) {
    uint32_t requested_start, requested_end, cached_start, cached_end;
    int dpu_index = 0;
    requested_start = xfer_matrix->offset_in_mram;
    requested_end = requested_start + xfer_matrix->size;
    cached_start = rank->xfer_cache.offset_in_mram;
    cached_end = cached_start + rank->xfer_cache.size;

    if (requested_start < cached_start || requested_end > cached_end) {
        return false;
    } 

    for (dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++) {
        if (xfer_matrix->ptr[dpu_index] != NULL) {
            if (rank->xfer_cache.xferp[dpu_index].nb_pages == 0) {
                return false;
            }       
        }
    }
    return true;
}


//If xfer size is larger than threshold, return false
//Else if cache hit, the function will populate the xfer_matrix with cache, return true
//Else if cache miss, the function will prefetch from the device, return true
bool dpu_prefetch_search_cache(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix){
    uint32_t xfer_nb_pages_per_dpu;
    struct dpu_transfer_mram *new_xfer_matrix; 
    struct dpu_region_address_translation *tr;

    xfer_nb_pages_per_dpu = ((xfer_matrix->size ) / PAGE_SIZE) + 1;
    if(xfer_nb_pages_per_dpu > DPU_PREFETCH_PAGE_THRESHOLD){
        //printk(KERN_CRIT "#### Case 1: If xfer size is larger than threshold, return false.\n");    
        return false;
    }
    if(count_xfer_dpu(xfer_matrix)==1){
        //serial transfer, we ignore that
        return false;
    }

    if(dpu_prefetch_cache_hit(rank, xfer_matrix)){
        //printk(KERN_CRIT "#### Case 2: Else if cache hit, the function will populate the xfer_matrix with cache, return true\n");
        populate_xfer_matrix_from_cache(rank, xfer_matrix);
    }
    else{
        //printk(KERN_CRIT "#### Case 3: Else if cache miss, the function will prefetch from the device, return true\n");
        new_xfer_matrix = kmalloc(sizeof(struct dpu_transfer_mram), GFP_KERNEL);
        construct_prefetch_xfer_matrix(rank, xfer_matrix, new_xfer_matrix);
        tr = &rank->region->addr_translate;
        tr->read_from_rank(tr, rank->region->base, rank->channel_id, new_xfer_matrix);
        rank->xfer_cache.offset_in_mram =new_xfer_matrix ->offset_in_mram;
        rank->xfer_cache.size = new_xfer_matrix->size;
        populate_xfer_matrix_from_cache(rank, xfer_matrix);
        kfree(new_xfer_matrix);
    }
    return true;
}

void dpu_prefetch_reset_cache(struct dpu_rank_t *rank){
    int dpu_index;
    rank->xfer_cache.size = 0;
    rank->xfer_cache.offset_in_mram = 0;
    for (dpu_index = 0; dpu_index < DPU_MAX_NR_DPUS; dpu_index++) {
        rank->xfer_cache.xferp[dpu_index].nb_pages = 0;
    }
}

#else
bool dpu_prefetch_search_cache(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix){
    return false;
}

void dpu_prefetch_reset_cache(struct dpu_rank_t *rank){
    return;
}
#endif