#ifndef DPU_PREFETCH_H
#define DPU_PREFETCH_H
#define DPU_PREFETCH_ENABLE 0
#include "dpu_rank.h"
bool dpu_prefetch_search_cache(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix);
void dpu_prefetch_reset_cache(struct dpu_rank_t *rank);
#endif