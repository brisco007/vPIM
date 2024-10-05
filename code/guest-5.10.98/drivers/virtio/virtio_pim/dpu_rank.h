#ifndef DPU_RANK_H
#define DPU_RANK_H
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sizes.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include "dpu_types.h"
#include "dpu_region_address_translation.h"
#include "dpu_vpd_structures.h"
#define DPU_RANK_NAME "dpu_rank"
#define DPU_RANK_PATH DPU_RANK_NAME "%d"
#define DPU_RANK_MCU_VERSION_LEN 128
#define DPU_DIMM_PART_NUMBER_LEN 20
#define DPU_DIMM_SERIAL_NUMBER_LEN 10
#define DPU_RANK_INVALID_INDEX 255
/* Size in bytes of one rank of a DPU DIMM */
#define DPU_RANK_SIZE (8ULL * SZ_1G)
/* The granularity of access to a rank is a cache line, which is 64 bytes */
#define DPU_RANK_SIZE_ACCESS 64


extern struct class *dpu_rank_class;
extern const struct attribute_group *dpu_rank_attrs_groups[];

typedef enum request_type{
	REQ_CONFIG,
    REQ_WRITE_TO_RANK,
    REQ_READ_FROM_RANK,
    REQ_COMMIT_COMMAND,
    REQ_UPDATE_COMMAND,
    REQ_UPDATE_STATE,
    REQ_FREE_STATE,
	REQ_COMMIT_COMMAND_BATCH,
	REQ_WRITE_TO_RANK_BATCH,
} request_type_t;

struct Request{
    request_type_t request_type;
    size_t payload_size;
	int index;
};

struct Response{
    request_type_t request_type;
    size_t payload_size;
};

struct xfer_page {
	struct page **pages;
	unsigned long nb_pages;
	/* Because user allocation through malloc
	 * can be unaligned to page size, we must
	 * know the offset within the first page of
	 * the buffer.
	 */
	int off_first_page;
};

struct dpu_configuration_slice_info_t {
	uint64_t byte_order;
	uint64_t structure_value;
	struct dpu_slice_target slice_target;
	dpu_bitfield_t host_mux_mram_state;
	dpu_selected_mask_t dpus_per_group[DPU_MAX_NR_GROUPS];
	dpu_selected_mask_t enabled_dpus;
	bool all_dpus_are_enabled;
};

struct dpu_control_interface_context {
	dpu_ci_bitfield_t fault_decode;
	dpu_ci_bitfield_t fault_collide;

	dpu_ci_bitfield_t color;
	struct dpu_configuration_slice_info_t slice_info
		[DPU_MAX_NR_CIS]; // Used for the current application to hold slice info
};

struct dpu_run_context_t {
	dpu_bitfield_t dpu_running[DPU_MAX_NR_CIS];
	dpu_bitfield_t dpu_in_fault[DPU_MAX_NR_CIS];
	uint8_t nb_dpu_running;
};

struct dpu_runtime_state_t {
	struct dpu_control_interface_context control_interface;
	struct dpu_run_context_t run_context;
};

struct dpu_rank_owner {
	uint8_t is_owned;
	unsigned int usage_count;
};

struct dpu_t {
	struct dpu_rank_t *rank;
	dpu_slice_id_t slice_id;
	dpu_member_id_t dpu_id;
	bool enabled;
};

struct dpu_transfer_info {
	uint32_t offset_in_mram;
	uint32_t size;
};

struct xfer_page_info{
	unsigned long nb_pages;
	int off_first_page;
};


#define CI_BATCH_SIZE 4
#define XFER_BATCH_SIZE 32
#define DPU_RANK_PAGES 4096
#define DPU_BATCH_PAGE_THRESHOLD DPU_RANK_PAGES/MAX_NR_DPUS_PER_RANK

struct xfer_page_batch {
	struct page **pages;
	struct xfer_page_info xferp_infos[XFER_BATCH_SIZE];
};

struct dpu_transfer_mram_batch {
	struct xfer_page_batch xferpb[DPU_MAX_NR_DPUS];
	struct dpu_transfer_info matrix_infos[XFER_BATCH_SIZE];
};

//Each DPU has a cache of 16 pages
#define DPU_PREFETCH_CACHE_PAGES 16
#define DPU_PREFETCH_SIZE 4
#define DPU_PREFETCH_PAGE_THRESHOLD DPU_PREFETCH_CACHE_PAGES/DPU_PREFETCH_SIZE

struct dpu_transfer_prefetch_cache{
	uint32_t offset_in_mram;
	uint32_t size;
	struct xfer_page xferp[DPU_MAX_NR_DPUS];
};

struct dpu_rank_t{
	struct list_head list;
	struct dpu_region *region;
	struct cdev cdev;
	struct device dev;
	struct dpu_rank_owner owner;
	struct dpu_runtime_state_t runtime;
	struct dpu_bit_config bit_config;
    struct dpu_t *dpus;
	uint8_t id;
	uint8_t channel_id;
	uint8_t slot_index;
	uint8_t debug_mode;
	uint64_t control_interface[DPU_MAX_NR_CIS];
	uint64_t data[DPU_MAX_NR_CIS];
	struct page **xfer_dpu_page_array;
	struct xfer_page xfer_pg[DPU_MAX_NR_DPUS];
	uint8_t rank_index;
	uint8_t rank_count;
	char* mcu_version;
	char* part_number;
	char* serial_number;
	struct dpu_vpd vpd;
	struct mutex rank_lock;
	uint64_t ci_batch[DPU_MAX_NR_CIS * CI_BATCH_SIZE];
	uint32_t ci_batch_index;
	struct dpu_transfer_mram_batch xfer_batch;
	uint32_t rank_batch_index;
	struct page **rank_pages;
	uint32_t rank_page_index;
	struct dpu_transfer_prefetch_cache xfer_cache;
};

struct dpu_rank_handler {
	u32 (*commit_commands)(struct dpu_rank_t *rank, uint64_t *buffer);
	u32 (*update_commands)(struct dpu_rank_t *rank, uint64_t *buffer, uint32_t request_id);
};

void  dpu_rank_release_device(struct dpu_region *region);
int dpu_rank_init_device(struct device *dev, struct dpu_region *region, bool must_init_mram);
int dpu_rank_handle_read_rank_response(int id);
int dpu_rank_handle_update_response(int id);
#endif