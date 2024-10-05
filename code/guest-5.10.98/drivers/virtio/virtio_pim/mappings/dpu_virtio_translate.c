/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
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
#include <linux/ktime.h>
#include "../dpu_rank.h"
#include "../dpu_region_address_translation.h"
#include "../dpu.h"
#include "../dpu_vpd_structures.h"

int count = 0;
int nb_request = 0;
int is_first_own=1;
int ci_count=0;
//int backend_is_owned;

struct CI_array{
	uint64_t control_interface[DPU_MAX_NR_CIS];
};

struct dpu_rank_request_props {
	uint8_t is_owned;
	uint8_t rank_count;
	uint8_t channel_id;
	uint8_t backend_id;
	uint8_t dpu_chip_id;
	uint8_t rank_id;
	uint64_t capabilities;
	uint8_t mode;
	uint8_t debug_mode;
	uint8_t rank_index;
	uint32_t usage_count;
	uint8_t mcu_version[DPU_RANK_MCU_VERSION_LEN];
	uint8_t part_number[DPU_DIMM_PART_NUMBER_LEN];
	uint8_t dimm_sn[DPU_DIMM_SERIAL_NUMBER_LEN];
};


int virtio_init_rank(struct dpu_region_address_translation *tr,
		      uint8_t channel_id);
void virtio_destroy_rank(struct dpu_region_address_translation *tr,
			  uint8_t channel_id);
void virtio_write_to_cis(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data);

void virtio_write_to_cis_batch(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data);

void virtio_read_from_cis(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   void *block_data);
void virtio_request_config(struct dpu_region_address_translation *tr);
void virtio_request_run_config(struct dpu_region_address_translation *tr);
void virtio_request_free_rank(struct dpu_region_address_translation *tr);
void virtio_write_to_rank_address(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix);
void virtio_write_to_rank_batch(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id);

void virtio_read_from_rank_address(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix);
struct dpu_region_address_translation virtio_translate = {
	.desc = {
		.topology.nr_of_control_interfaces = 8,
		.topology.nr_of_dpus_per_control_interface = 8,
		.memories.mram_size = 64 * SZ_1M,
		.memories.wram_size = 64 * SZ_1K,
		.memories.iram_size = SZ_4K,
		.timings.fck_frequency_in_mhz = 800,
		.timings.carousel.cmd_duration = 2,
		.timings.carousel.cmd_sampling = 1,
		.timings.carousel.res_duration = 2,
		.timings.carousel.res_sampling = 1,
		.timings.reset_wait_duration = 20,
		.timings.clock_division = 4,
		.timings.std_temperature = 110,
		.dpu.nr_of_threads = 24,
		.dpu.nr_of_atomic_bits = 256,
		.dpu.nr_of_notify_bits = 40,
		.dpu.nr_of_work_registers_per_thread = 24,
	},
	.backend_id = DPU_BANKEND_VIRTIO,
	.capabilities = CAP_SAFE,
	.init_rank = virtio_init_rank,
	.destroy_rank = virtio_destroy_rank,
	.write_to_rank = virtio_write_to_rank_address,
	.write_to_rank_batch = virtio_write_to_rank_batch,
	.read_from_rank = virtio_read_from_rank_address,
	.write_to_cis = virtio_write_to_cis,
	.write_to_cis_batch = virtio_write_to_cis_batch,
	.read_from_cis = virtio_read_from_cis,
	.request_config = virtio_request_config,
	.realloc_config = virtio_request_run_config,
	.free_rank = virtio_request_free_rank
};


void format_virtio_string(char * array, int len) {
	int i = 0;
	for (i = 0; i <len; i++) {
		if(array[i] == '\n' || array[i] == '\t' ){
			array[i] = '\0';
			break;
		}			
	}
}

void print_xfer_page(void *address){
	int k;
	for (k = 0; k< SZ_4K; k++) {
		if(((uint8_t *) address)[k] != 0){
			printk(KERN_CRIT "[MYYYYYYYYYYY]%d, at index : %d", ((uint8_t *) address)[k],k);
		}
	}
}

void virtio_send_xfer_matrix(struct dpu_region_address_translation *tr, 
				request_type_t type, struct dpu_transfer_mram *xfer_matrix){
	struct Request *req;
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	int nb_cis = tr->desc.topology.nr_of_control_interfaces;
	int nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;
	struct xfer_page *xferp;
	struct page *cur_page;
	uint64_t address;
	struct scatterlist request_scatterlist, trans_scatterlist;
	struct scatterlist *outlist;
	//count for number of buffers to be transfered
	int nb_total_buffers = 0;
	//the request type and the meta data of xfer_matrix
	struct dpu_transfer_info *trans_info;
	//a list of xfer_page meta data
	struct xfer_page_info *page_info;
	//a list of pointers pointing to the data to be transfered
	void **payload_list;
	//the corresponding size for each element of the former payload_list
	size_t *sizelist;
	//the index for the last two lists
	int out_index = 0;
	int i, j;
	//int k;
	unsigned int len;

	ktime_t start, end, interrupt;
	start = ktime_get();

	//count the number of buffers we need to send
	for(i=0; i<nb_cis*nb_dpus_per_ci; i++){
		xferp = xfer_matrix->ptr[i];
		nb_total_buffers++;
		if(!xferp) continue; 
		//nb_total_buffers+=count_num_tables(xferp->nb_pages);
		nb_total_buffers++;
	}
	//allocation 
	req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = type;
    req->payload_size = nb_total_buffers;
	req->index = count++;

	//the meta data struct for xfer_matrix
	trans_info = kzalloc(sizeof(struct dpu_transfer_info), GFP_KERNEL);
	trans_info->offset_in_mram = xfer_matrix->offset_in_mram;
	trans_info->size = xfer_matrix->size;
	//we have 64 page_info structs
	page_info = kzalloc(nb_cis*nb_dpus_per_ci*sizeof(struct xfer_page_info), GFP_KERNEL);
	//total number of buffers for transfer
	outlist = kzalloc(nb_total_buffers*sizeof(struct scatterlist), GFP_KERNEL);
	//the payload for the former outlist
	payload_list = kzalloc(nb_total_buffers*sizeof(void *), GFP_KERNEL);
	//the size list for each element for the former payload list
	sizelist = kzalloc(nb_total_buffers*sizeof(size_t), GFP_KERNEL);
	
	for(i=0; i<nb_cis*nb_dpus_per_ci; i++){
		//the current column (dpu) of the matrix
		xferp = xfer_matrix->ptr[i];
		if(!xferp){
			page_info[i].nb_pages = 0;
			page_info[i].off_first_page = 0;
			sizelist[out_index] = sizeof(struct xfer_page_info);
			payload_list[out_index] = (void *) (page_info + i);
			out_index++;
		} else{
			page_info[i].nb_pages = xferp->nb_pages;
			page_info[i].off_first_page = xferp->off_first_page;
			sizelist[out_index] = sizeof(struct xfer_page_info);
			payload_list[out_index] = (void *) (page_info + i);
			out_index++;
			sizelist[out_index] = xferp->nb_pages*sizeof(uint64_t);
			payload_list[out_index] = kzalloc(xferp->nb_pages*sizeof(uint64_t), GFP_KERNEL);
			for(j=0; j<xferp->nb_pages; j++){
				cur_page = xferp->pages[j];
				address = page_to_phys(cur_page);
				((uint64_t *) payload_list[out_index])[j]= address;
			}
			out_index++;
		}
	}
	sg_init_one(&request_scatterlist, req, sizeof(struct Request));
	sg_init_one(&trans_scatterlist, trans_info, sizeof(struct dpu_transfer_info));
	for(i=0;i<nb_total_buffers; i++){
		sg_init_one(&outlist[i], payload_list[i], sizelist[i]);
	}
	virtqueue_add_outbuf(vpim_device_info->vq, &request_scatterlist, 1, req, GFP_KERNEL);
	virtqueue_add_outbuf(vpim_device_info->vq, &trans_scatterlist, 1, trans_info, GFP_KERNEL);
	for(i=0;i<nb_total_buffers; i++){
		virtqueue_add_outbuf(vpim_device_info->vq, &outlist[i], 1, payload_list[i], GFP_KERNEL);
	}

	end = ktime_sub(ktime_get(), start);
    //kick the queue 
	rank_lock(rank);
	interrupt = ktime_get();
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	for(i=0;i<nb_total_buffers+2;i++){
		virtqueue_get_buf(rank->region->device_info->vq, &len);
	}

	interrupt = ktime_sub(ktime_get(), interrupt);
	//printk(KERN_CRIT "#### Serialization time %llu ns\n", ktime_to_ms(end));
	//printk(KERN_CRIT "#### Firecraker time %llu ms\n", ktime_to_ms(interrupt));

	kfree(req);
	kfree(trans_info);
	kfree(page_info);
	kfree(outlist);
	kfree(payload_list);
	kfree(sizelist);

}

void virtio_write_to_rank_batch(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id){
	struct Request *req;
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	int nb_cis = tr->desc.topology.nr_of_control_interfaces;
	int nb_dpus_per_ci = tr->desc.topology.nr_of_dpus_per_control_interface;
	struct xfer_page_batch *xferpb;
	struct page *cur_page;
	uint64_t address;
	struct scatterlist *outlist;
	//count for number of buffers to be transfered
	int nb_total_buffers;
	//a list of pointers pointing to the data to be transfered
	void **payload_list;
	//the corresponding size for each element of the former payload_list
	size_t *sizelist;
	//the index for the last two lists
	int out_index = 0;
	int i, j;
	uint32_t total_pages = 0;
	//int k;
	unsigned int len;
	struct dpu_transfer_mram_batch *batch = &rank->xfer_batch;
	//count nb_total_buffers
	nb_total_buffers = 1 + 1 + nb_cis * nb_dpus_per_ci;
	for(i=0; i<nb_cis*nb_dpus_per_ci; i++){
		xferpb = &batch->xferpb[i];
		total_pages = 0;
		for(j=0; j<rank->rank_batch_index; j++){
			total_pages += xferpb->xferp_infos[j].nb_pages;
		}
		if(total_pages>0) nb_total_buffers++;
	}
	//allocation 
	req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_WRITE_TO_RANK_BATCH;
    req->payload_size = rank->rank_batch_index; //FIXME : Not the nb_total_buffers?
	req->index = count++;
	outlist = kzalloc(nb_total_buffers*sizeof(struct scatterlist), GFP_KERNEL);
	//the payload for the former outlist
	payload_list = kzalloc(nb_total_buffers*sizeof(void *), GFP_KERNEL);
	//the size list for each element for the former payload list
	sizelist = kzalloc(nb_total_buffers*sizeof(size_t), GFP_KERNEL);
	
	//fill up the payload_list and sizelist
	//the request
	payload_list[out_index] = (void *) req;
	sizelist[out_index] = sizeof(struct Request);
	out_index++;
	//the array of dpu_transfer_info, matrix_infos
	payload_list[out_index] = (void *) &batch->matrix_infos;
	sizelist[out_index] = XFER_BATCH_SIZE * sizeof(struct dpu_transfer_info);
	out_index++;
	/*
	printk(KERN_CRIT "#### Kernel: dpu_transfer_info\n");
	for(i=0;i<XFER_BATCH_SIZE; i++){
		printk(KERN_CRIT "#### offset: %d, size: %d\n", batch->matrix_infos[i].offset_in_mram, batch->matrix_infos[i].size);
	}
	*/
	
	//the xferp_info and pages for each dpu
	for(i=0; i<nb_cis*nb_dpus_per_ci; i++){
		xferpb = &batch->xferpb[i];
		payload_list[out_index] = (void *) (&xferpb->xferp_infos);
		sizelist[out_index] = XFER_BATCH_SIZE * sizeof(struct xfer_page_info);
		out_index++;
		total_pages = 0;
		for(j=0; j<rank->rank_batch_index; j++){
			total_pages += xferpb->xferp_infos[j].nb_pages;
		}
		if(total_pages==0) continue;
		/*
		printk(KERN_CRIT "#### Kernel: xfer_info\n");
		for(j=0;j<XFER_BATCH_SIZE; j++){
			printk(KERN_CRIT "#### nb_pages: %ld, offset: %d\n", xferpb->xferp_infos[j].nb_pages, xferpb->xferp_infos[j].off_first_page);
		}
		*/
		sizelist[out_index] = total_pages*sizeof(uint64_t);
		payload_list[out_index] = kzalloc(total_pages*sizeof(uint64_t), GFP_KERNEL);
		for(j=0; j<total_pages; j++){
			cur_page = xferpb->pages[j];
			address = page_to_phys(cur_page);
			
			((uint64_t *) payload_list[out_index])[j]= address;
			
			/*
			printk(KERN_CRIT "#### address: %llu\n", address);
			uint8_t *page_ptr = kmap(cur_page);
			int k;
			for(k=0;k<50; k++){
				printk(KERN_CRIT "#### %u\n", page_ptr[k]);
			}
			kunmap(cur_page);
			*/

		}
		out_index++;
	}
	//init the scatterlists
	for(i=0;i<nb_total_buffers; i++){
		sg_init_one(&outlist[i], payload_list[i], sizelist[i]);
	}
	//assign payload to scatterlists
	for(i=0;i<nb_total_buffers; i++){
		virtqueue_add_outbuf(vpim_device_info->vq, &outlist[i], 1, payload_list[i], GFP_KERNEL);
	}
	rank_lock(rank);
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	for(i=0;i<nb_total_buffers;i++){
		virtqueue_get_buf(rank->region->device_info->vq, &len);
	}
	kfree(req);
	kfree(outlist);
	kfree(payload_list);
	kfree(sizelist);
}

void virtio_write_to_rank_address(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix){
	virtio_send_xfer_matrix(tr, REQ_WRITE_TO_RANK, xfer_matrix);
}

void virtio_read_from_rank_address(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   struct dpu_transfer_mram *xfer_matrix){
	virtio_send_xfer_matrix(tr, REQ_READ_FROM_RANK, xfer_matrix);
}

void virtio_write_to_cis_batch(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data)
{	
	void **payload_list;
	struct Request *req;
	struct scatterlist sg_out;
	struct scatterlist *outlist;
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	uint32_t batch_size = rank->ci_batch_index;
	uint32_t i, j;
	unsigned int len;
	req = kzalloc(sizeof(struct Request), GFP_KERNEL);
	req->request_type = REQ_COMMIT_COMMAND_BATCH;
    req->payload_size = batch_size;
	req->index = count++;
	outlist = kzalloc(batch_size*sizeof(struct scatterlist), GFP_KERNEL);
	payload_list = kzalloc(batch_size*sizeof(void *), GFP_KERNEL);
	for(i=0; i<batch_size; i++){
		payload_list[i] = kzalloc(sizeof(struct CI_array), GFP_KERNEL);
		for(j=0; j<DPU_MAX_NR_CIS; j++){
			((struct CI_array *) payload_list[i])->control_interface[j] = ((uint64_t *) block_data)[i*DPU_MAX_NR_CIS+j];
		}
	}
	sg_init_one(&sg_out, req, sizeof(struct Request));
	virtqueue_add_outbuf(vpim_device_info->vq, &sg_out, 1, req, GFP_KERNEL);
	for(i=0;i<batch_size;i++){
		sg_init_one(&outlist[i], payload_list[i], sizeof(struct CI_array));
		virtqueue_add_outbuf(vpim_device_info->vq, &outlist[i], 1, payload_list[i], GFP_KERNEL);
	}
    //kick the queue 
	rank_lock(rank);
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	for(i=0; i<batch_size+1; i++){
		virtqueue_get_buf(rank->region->device_info->vq, &len);
	}	
	kfree(req);
	kfree(payload_list);
	ci_count++;
	rank->ci_batch_index = 0;
	//if(ci_count%4000==0) printk(KERN_CRIT "#### nb CI: %d\n", ci_count);
}


void virtio_write_to_cis(struct dpu_region_address_translation *tr,
			  void *base_region_addr, uint8_t channel_id,
			  void *block_data)
{
	struct Request *req;
    struct scatterlist sg_out, sg_out_payload; 
	struct dpu_rank_t *rank = &tr->region->rank;
	struct CI_array *ci_array = kzalloc(sizeof(struct CI_array), GFP_KERNEL);
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	int i;
	unsigned int len;
	req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_COMMIT_COMMAND;
    req->payload_size = sizeof(struct CI_array);
	req->index = count++;
	for(i=0; i<DPU_MAX_NR_CIS; i++){
		ci_array->control_interface[i] = ((uint64_t *) block_data)[i];
	}
	//construct the scatter list
    sg_init_one(&sg_out, req, sizeof(struct Request));
    sg_init_one(&sg_out_payload, ci_array, sizeof(struct CI_array));
    // add the request to the queue, in_buf is sent as the buffer idetifier 
	virtqueue_add_outbuf(vpim_device_info->vq, &sg_out, 1, req, GFP_KERNEL);
	virtqueue_add_outbuf(vpim_device_info->vq, &sg_out_payload, 1, ci_array, GFP_KERNEL);	
    //kick the queue 
	rank_lock(rank);
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	memcpy(req, 
			(struct Request *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct Request));
	virtqueue_get_buf(rank->region->device_info->vq, &len);
	kfree(req);
	kfree(ci_array);
	ci_count++;
	//if(ci_count%4000==0) printk(KERN_CRIT "#### nb CI: %d\n", ci_count);
}


void virtio_read_from_cis(struct dpu_region_address_translation *tr,
			   void *base_region_addr, uint8_t channel_id,
			   void *block_data)
{

	struct Request *req;
    struct scatterlist list_in_payload, list_out;
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	struct CI_array *output;
	unsigned int len;
	output = kzalloc(sizeof(struct CI_array), GFP_KERNEL);
    //construct the request
    req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_UPDATE_COMMAND;
    req->payload_size = 0;
	req->index = count++;
    //construct the empty response
	//construct the scatter list
    sg_init_one(&list_out, req, sizeof(struct Request));
	sg_init_one(&list_in_payload, output, sizeof(struct CI_array));
    // add the request to the queue, in_buf is sent as the buffer idetifier 
    virtqueue_add_outbuf(vpim_device_info->vq, &list_out, 1, req, GFP_KERNEL);
    virtqueue_add_inbuf(vpim_device_info->vq, &list_in_payload, 1, output, GFP_KERNEL);
	rank_lock(rank);
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	memcpy(req, 
			(struct Request *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct Request));
	memcpy(block_data, 
			&(((struct CI_array *) virtqueue_get_buf(rank->region->device_info->vq, &len))->control_interface),
			DPU_MAX_NR_CIS*sizeof(uint64_t));
	kfree(req);
	kfree(output);
	ci_count++;
	//if(ci_count%4000==0) printk(KERN_CRIT "#### nb CI: %d\n", ci_count);
}


int virtio_init_rank(struct dpu_region_address_translation *tr,
		      uint8_t channel_id){
	return 0;
}

void virtio_destroy_rank(struct dpu_region_address_translation *tr,
			  uint8_t channel_id){
	return;
}



void virtio_request_config(struct dpu_region_address_translation *tr){
	struct Request *req;
	unsigned int len;
	struct dpu_runtime_state_t *runtime;
	struct dpu_bit_config *bit_config;
    struct scatterlist sg_in1, sg_in2, sg_in3, sg_in4, sg_in5, sg_out; 
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	//struct for other ranks attr;
	struct dpu_rank_request_props* rank_props;
	struct dpu_vpd *vpd;
	struct dpu_hw_description_t *hw; 
    //construct the request
    req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_CONFIG;
    req->payload_size = 0;
	req->index = count++;
    //construct the empty response
	runtime = kzalloc(sizeof(struct dpu_runtime_state_t), GFP_KERNEL);
	bit_config = kzalloc(sizeof(struct dpu_bit_config), GFP_KERNEL);
	hw = kzalloc(sizeof(struct dpu_hw_description_t), GFP_KERNEL);
	rank_props = kzalloc(sizeof(struct dpu_rank_request_props), GFP_KERNEL);
	vpd = kzalloc(sizeof(struct dpu_vpd), GFP_KERNEL);
	//construct the scatter list
    sg_init_one(&sg_out, req, sizeof(struct Request));
    sg_init_one(&sg_in1, runtime, sizeof(struct dpu_runtime_state_t));
	sg_init_one(&sg_in2, bit_config, sizeof(struct dpu_bit_config));
	sg_init_one(&sg_in3, hw, sizeof(struct dpu_hw_description_t));
	sg_init_one(&sg_in4, rank_props, sizeof(struct dpu_rank_request_props));
	sg_init_one(&sg_in5, vpd, sizeof(struct dpu_vpd));
    // add the request to the queue, in_buf is sent as the buffer idetifier 
    virtqueue_add_outbuf(vpim_device_info->vq, &sg_out, 1, req, GFP_KERNEL);
    virtqueue_add_inbuf(vpim_device_info->vq, &sg_in1, 1, runtime, GFP_KERNEL);
    virtqueue_add_inbuf(vpim_device_info->vq,&sg_in2, 1, bit_config,GFP_KERNEL);
	virtqueue_add_inbuf(vpim_device_info->vq,&sg_in3, 1, hw,  GFP_KERNEL);
	virtqueue_add_inbuf(vpim_device_info->vq,&sg_in4, 1, rank_props,  GFP_KERNEL);
	virtqueue_add_inbuf(vpim_device_info->vq,&sg_in5, 1, vpd,  GFP_KERNEL);

	rank_lock(rank);
	virtqueue_kick(vpim_device_info->vq);
	rank_lock(rank);
	rank_unlock(rank);
	
	req = (struct Request *) virtqueue_get_buf(rank->region->device_info->vq, &len);
	
	memcpy(&rank->runtime, 
			(struct dpu_runtime_state_t *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct dpu_runtime_state_t));

	memcpy(&rank->bit_config, 
			(struct dpu_bit_config *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct dpu_bit_config));

	memcpy(&tr->desc, 
			(struct dpu_hw_description_t *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct dpu_hw_description_t));

	memcpy(rank_props, 
			(struct dpu_rank_request_props *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct dpu_rank_request_props));

	memcpy(vpd, 
			(struct dpu_vpd *) virtqueue_get_buf(rank->region->device_info->vq, &len),
			sizeof(struct dpu_vpd));

	format_virtio_string( (char *) rank_props->mcu_version, 128);
	format_virtio_string( (char *) rank_props->part_number, 20);
	format_virtio_string( (char *) rank_props->dimm_sn, 10);

	rank->owner.is_owned = rank_props->is_owned;
	rank->owner.usage_count = rank_props->usage_count;
	rank->rank_count = rank_props->rank_count;
	rank->id = rank_props->rank_id;
	rank->channel_id = rank_props->channel_id;
	tr->backend_id = rank_props->backend_id;
	tr->desc.signature.chip_id = rank_props->dpu_chip_id;
	rank->region->mode = rank_props->mode;
	rank->debug_mode = rank_props->debug_mode;
	rank->rank_index = rank_props->rank_index;
	rank->mcu_version =  (char *)rank_props->mcu_version;
	rank->part_number =  (char *)rank_props->part_number;
	rank->serial_number =  (char *) rank_props->dimm_sn;
	rank->vpd = *vpd;
}


 
void virtio_request_run_config(struct dpu_region_address_translation *tr){

	if(is_first_own == 1){
		is_first_own = 0;
	} else {
		struct Request *req;
		unsigned int len;
		struct dpu_runtime_state_t *runtime;
		struct dpu_bit_config *bit_config;
		struct scatterlist sg_in1, sg_in2, sg_in3, sg_in4, sg_in5, sg_out; 
		struct dpu_rank_t *rank = &tr->region->rank;
		vpim_device_info_t *vpim_device_info = rank->region->device_info;
		//struct for other ranks attr;
		struct dpu_rank_request_props* rank_props;
		struct dpu_vpd *vpd;
		struct dpu_hw_description_t *hw; 
		//construct the request
		req = kzalloc(sizeof(struct Request), GFP_KERNEL);
		req->request_type = REQ_UPDATE_STATE;
		req->payload_size = 0;
		req->index = count++;
		//construct the empty response
		runtime = kzalloc(sizeof(struct dpu_runtime_state_t), GFP_KERNEL);
		bit_config = kzalloc(sizeof(struct dpu_bit_config), GFP_KERNEL);
		hw = kzalloc(sizeof(struct dpu_hw_description_t), GFP_KERNEL);
		rank_props = kzalloc(sizeof(struct dpu_rank_request_props), GFP_KERNEL);
		vpd = kzalloc(sizeof(struct dpu_vpd), GFP_KERNEL);
		//construct the scatter list
		sg_init_one(&sg_out, req, sizeof(struct Request));
		sg_init_one(&sg_in1, runtime, sizeof(struct dpu_runtime_state_t));
		sg_init_one(&sg_in2, bit_config, sizeof(struct dpu_bit_config));
		sg_init_one(&sg_in3, hw, sizeof(struct dpu_hw_description_t));
		sg_init_one(&sg_in4, rank_props, sizeof(struct dpu_rank_request_props));
		sg_init_one(&sg_in5, vpd, sizeof(struct dpu_vpd));
		// add the request to the queue, in_buf is sent as the buffer idetifier 
		virtqueue_add_outbuf(vpim_device_info->controlq, &sg_out, 1, req, GFP_KERNEL);
		virtqueue_add_inbuf(vpim_device_info->controlq, &sg_in1, 1, runtime, GFP_KERNEL);
		virtqueue_add_inbuf(vpim_device_info->controlq,&sg_in2, 1, bit_config,GFP_KERNEL);
		virtqueue_add_inbuf(vpim_device_info->controlq,&sg_in3, 1, hw,  GFP_KERNEL);
		virtqueue_add_inbuf(vpim_device_info->controlq,&sg_in4, 1, rank_props,  GFP_KERNEL);
		virtqueue_add_inbuf(vpim_device_info->controlq,&sg_in5, 1, vpd,  GFP_KERNEL);

		control_lock();
		virtqueue_kick(vpim_device_info->controlq);
		control_lock();
		control_unlock();
		
		req = (struct Request *) virtqueue_get_buf(rank->region->device_info->controlq, &len);
		memcpy(&rank->runtime, 
				(struct dpu_runtime_state_t *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
				sizeof(struct dpu_runtime_state_t));

		memcpy(&rank->bit_config, 
				(struct dpu_bit_config *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
				sizeof(struct dpu_bit_config));

		memcpy(&tr->desc, 
				(struct dpu_hw_description_t *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
				sizeof(struct dpu_hw_description_t));

		memcpy(rank_props, 
				(struct dpu_rank_request_props *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
				sizeof(struct dpu_rank_request_props));

		memcpy(vpd, 
				(struct dpu_vpd *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
				sizeof(struct dpu_vpd));

		format_virtio_string( (char *) rank_props->mcu_version, 128);
		format_virtio_string( (char *) rank_props->part_number, 20);
		format_virtio_string( (char *) rank_props->dimm_sn, 10);

		rank->owner.is_owned = rank_props->is_owned;
		rank->owner.usage_count = rank_props->usage_count;
		rank->rank_count = rank_props->rank_count;
		rank->id = rank_props->rank_id;
		rank->channel_id = rank_props->channel_id;
		tr->backend_id = rank_props->backend_id;
		tr->desc.signature.chip_id = rank_props->dpu_chip_id;
		rank->region->mode = rank_props->mode;
		rank->debug_mode = rank_props->debug_mode;
		rank->rank_index = rank_props->rank_index;
		rank->mcu_version =  (char *)rank_props->mcu_version;
		rank->part_number =  (char *)rank_props->part_number;
		rank->serial_number =  (char *) rank_props->dimm_sn;
		rank->vpd = *vpd;
	}
}

void virtio_request_free_rank(struct dpu_region_address_translation *tr){

	struct Request *req;
    struct scatterlist sg_out; 
	struct dpu_rank_t *rank = &tr->region->rank;
	vpim_device_info_t *vpim_device_info = rank->region->device_info;
	int saved_index;
	unsigned int len;
	req = kzalloc(sizeof(struct Request), GFP_KERNEL);
    req->request_type = REQ_FREE_STATE;
    req->payload_size = 0;
	req->index = count++;
	saved_index = req->index;
	//construct the scatter list
    sg_init_one(&sg_out, req, sizeof(struct Request));
    // add the request to the queue, in_buf is sent as the buffer idetifier 
	virtqueue_add_outbuf(vpim_device_info->controlq, &sg_out, 1, req, GFP_KERNEL);
    //kick the queue 
	control_lock();
	virtqueue_kick(vpim_device_info->controlq);
	control_lock();
	control_unlock();
	memcpy(req, 
			(struct Request *) virtqueue_get_buf(rank->region->device_info->controlq, &len),
			sizeof(struct Request));
	virtqueue_get_buf(rank->region->device_info->controlq, &len);
	kfree(req);
}


#define BANK_START(dpu_id)                                                     \
	(0x40000 * ((dpu_id) % 4) + ((dpu_id >= 4) ? 0x40 : 0))
// For each 64bit word, you must jump 16 * 64bit (2 cache lines)
#define BANK_OFFSET_NEXT_DATA(i) (i * 16)
#define BANK_CHUNK_SIZE 0x20000
#define BANK_NEXT_CHUNK_OFFSET 0x100000
#define XFER_BLOCK_SIZE 8
DEFINE_MUTEX(mutex_nb_ranks_allocated);
uint32_t nb_ranks_allocated = 0;
#define PREFETCH_MSR 0x1A4
#define PREFETCH_DISABLE 0xF
#define PREFETCH_ENABLE 0x0

