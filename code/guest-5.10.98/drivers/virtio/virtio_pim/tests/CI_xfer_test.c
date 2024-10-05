#include "../dpu.h"
#include "../dpu_rank.h"
#include "CI_xfer_test.h"

void read_from_ci_test(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
	uint32_t size_command;
	uint8_t nb_cis;
    uint64_t CIS[DPU_MAX_NR_CIS];
    int i = 0;
	printk(KERN_CRIT "########### Testing read from cis\n");
	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	size_command = sizeof(uint64_t) * nb_cis;


	memset(CIS, 0, size_command);
	
	tr->read_from_cis(tr, rank->region->base, rank->channel_id, CIS);
    printk(KERN_INFO "########### CIS:");
    for(i=0; i<nb_cis; i++){
        if(i<nb_cis-1) printk(KERN_INFO "%llu ", CIS[i]);
        else printk(KERN_INFO "%llu\n", CIS[i]);
    }
}

void write_to_ci_test(struct dpu_rank_t *rank){
    struct dpu_region_address_translation *tr;
	uint32_t size_command;
	uint8_t nb_cis;
    uint64_t CIS[DPU_MAX_NR_CIS];
	printk(KERN_CRIT "########### Testing write to cis\n");
	tr = &rank->region->addr_translate;
	nb_cis = tr->desc.topology.nr_of_control_interfaces;
	size_command = sizeof(uint64_t) * nb_cis;
	memset(CIS, 0, size_command);
	tr->read_from_cis(tr, rank->region->base, rank->channel_id, CIS);
    tr->write_to_cis(tr, rank->region->base, rank->channel_id, CIS);
}