#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/virtio.h>
#include <linux/types.h>
#include "dpu_config.h"
#include "dpu.h"
#include "dpu_types.h"
#include "dpu_hw_description.h"


//The function that configures the dpu_rank data structure
/*
* @brief: 
        Calls the corresponding function from the dpu_region_address_translation
		interface to request the virtual hardware specification from the hypervisor.
* @param: dpu_rank_t *rank the data structure to be filled up
* @return: the state
*/
uint32_t dpu_set_chip_id(struct dpu_rank_t *rank)
{
	struct dpu_region_address_translation *tr = &rank->region->addr_translate;
	struct dpu_bit_config *bit_config;
	uint32_t status = 0;
	bit_config = &tr->desc.dpu.pcb_transformation;

    tr->request_config(tr);
	return status;
}

