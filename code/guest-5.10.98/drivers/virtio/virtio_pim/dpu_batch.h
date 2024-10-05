#ifndef DPU_BATCH_H
#define DPU_BATCH_H
#include "dpu_rank.h"
#define DPU_BATCH_ENABLE 0
#define DPU_BATCH_CI_ENABLE 0
int dpu_batch_flush_CI(struct dpu_rank_t *rank);
int dpu_batch_write_to_cis(struct dpu_rank_t *rank);
int dpu_batch_write_to_rank(struct dpu_rank_t *rank, struct dpu_transfer_mram *xfer_matrix);
int dpu_batch_flush_rank(struct dpu_rank_t *rank);
#endif