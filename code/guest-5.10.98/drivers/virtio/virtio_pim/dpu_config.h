#ifndef DPU_CONFIG_H
#define DPU_CONFIG_H
#include "dpu.h"
#include "dpu_rank.h"
uint32_t dpu_set_chip_id(struct dpu_rank_t *rank);
uint32_t dpu_set_chip_id_response(void);
#endif