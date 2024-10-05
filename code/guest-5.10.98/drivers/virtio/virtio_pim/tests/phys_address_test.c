#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/virtio.h>
#include <linux/highmem.h>
#include <linux/wait.h>
#include "../dpu.h"
#include "../dpu_rank.h"
#include "phys_address_test.h"

#define REQ_ADDRESS_TEST 10

struct address_info{
    uint64_t address;
};

void print_page(void *address){
	int k;
	for (k = 0; k< SZ_4K; k++) {
		if(((uint8_t *) address)[k] != 0){
			printk(KERN_CRIT "[MYYYYYYYYYYY]%d, at index : %d", ((uint8_t *) address)[k],k);
		}
	}
}

void send_address_test(struct dpu_rank_t *rank){
    struct Request *req;
    struct scatterlist list_in_payload, list_out;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	unsigned int len;
    struct page *page;
    struct address_info *info;
    int k;
    page = alloc_pages(GFP_KERNEL, 0);
    for (k=1; k<=10; k++) {
        *((uint8_t *) page_to_virt(page) + k) = k;
    }
    print_page(page_to_virt(page));
    info = kzalloc(sizeof(struct address_info), GFP_KERNEL);
    info->address = page_to_phys(page);
    req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_ADDRESS_TEST;
    req->payload_size = 0;
    sg_init_one(&list_out, req, sizeof(struct Request));
	sg_init_one(&list_in_payload, info, sizeof(struct address_info));
    // add the request to the queue, in_buf is sent as the buffer idetifier 
    virtqueue_add_outbuf(vpim_device_info->vq, &list_out, 1, req, GFP_KERNEL);
    virtqueue_add_inbuf(vpim_device_info->vq, &list_in_payload, 1, info, GFP_KERNEL);
	printk(KERN_CRIT "########### Sending address %lld\n", info->address);
	ioctl_lock();
	virtqueue_kick(vpim_device_info->vq);
	ioctl_lock();
	ioctl_unlock();
    virtqueue_get_buf(rank->region->device_info->vq, &len);
    memcpy(info, (struct address_info *) virtqueue_get_buf(rank->region->device_info->vq, &len), sizeof(struct address_info));
    print_page(phys_to_virt(info->address));
}