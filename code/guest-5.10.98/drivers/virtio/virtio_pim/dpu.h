/*
 * Processing In Memory virtio driver
 *
 * Copyright UPMEM 2022
 * Copyright Dufy Teguia <brice.teguia_wakam@ens-lyon.fr>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

/*
 * ============================================================================
 *                             Device specification
 * ============================================================================
 *
 * 5.2 Example Device
 *
 * The vPIM device is a character device. Operations, that are requested are placed in a queue
 * and performed by the device that returns the exit status of the operation and the result.
 *
 * 5.2.1 Device ID
 *
 * 22
 *
 * 5.2.2 Virtqueues
 *
 * 0 requestq
 *
 * 5.2.3 Feature bits
 *
 * FEATURE_SAFE_MODE : The device is used in safe mode.
 * FEATURE_PERF_MODE : The device is used in performance mode.
 *
 * 5.2.4 Device configuration layout
 *
 * The device is actually a rank of the memory DIMM underneath. 
 * The device configuration layout is as follows:
 * - The structure vpim_config_t has the fields : rank_id, rank size (memory) and starting address
 * These are params that are passed to the device when it is initialized through the 
 * kernel command line.
 * - THe second structure dpu_region_t is the structure that represents the rank of the memory DIMM.
 * We can see the structure description in the file dpu_region.h
 *
 * 5.2.5 Device Initialization
 *
 * 1. Parse the kernel command line to get the rank_id, rank size and starting address
 * 2. Initialize the device with the rank_id, rank size and starting address
 * 3. The virtqueue is initialized.
 * 4. Initialize the dpu_region_t structure by sending a request to the device through virtqueue
 * 4. Create the device file in the /dev directory
 * 5. Register the device in the virtio PIM driver
 *
 * 5.2.6 Device Operation
 *
 * The device is used in safe mode. so when an ioctl is captured, it is forwarded to the device
 * using the virtqueu and the device returns the exit status and the result.
 * We build a request that contains the code of the request and the parameters of the request
 * and send it to the device.and expect a response. 
 *
 * 5.2.6.1 Driver Requirements: Device Operation
 *...
 * 5.2.6.2 Device Requirements: Device Operation
 *
 * ...
 */
//TODO : put this file in the include/uapi/linux/ directory
#ifndef DPU_REGION_H
#define DPU_REGION_H
#include <linux/virtio_ids.h>      /* virtio_ids.h */
#include <linux/pci.h>
#include <linux/platform_device.h>
#include "dpu_rank.h"
#include "dpu_region_address_translation.h"
//Virtio specific constants
#define VIRTIO_PIM_DEVICE_NAME "virtio_pim"
#define DPU_REGION_NAME "dpu_region"
#define FEATURE_SAFE_MODE 0
#define FEATURE_PERF_MODE 1
// Miscelanious
#define MAX_DATA_SIZE 20

extern int backend_is_owned;
typedef struct vpim_device_info_t {
    struct virtqueue *vq;
    struct virtqueue *controlq;
    struct dpu_region *region;
    struct virtio_device *vdev;
    struct platform_device *pdev;
    int in, out;
} vpim_device_info_t;

struct dpu_region {
    uint8_t mode;
	struct mutex lock;
	void *base; 
	uint64_t size;
	struct dentry *iladump;
	struct dentry *dpu_debugfs;
    struct dpu_region_address_translation addr_translate;
    void *mc_flush_address;
	uint8_t activate_ila;
	uint8_t activate_filtering_ila;
	uint8_t activate_mram_bypass;
	uint8_t spi_mode_enabled;
    uint32_t mram_refresh_emulation_period;
    struct dpu_rank_t rank;
    vpim_device_info_t *device_info;
};
extern struct dpu_region *region;
void ioctl_lock(void);
void ioctl_unlock(void);
int ioctl_islocked(void);
void control_lock(void);
void control_unlock(void);
int control_islocked(void);
void dpu_region_lock(struct dpu_region *region);
void dpu_region_unlock(struct dpu_region *region);
void find_new_virtqueue(vpim_device_info_t *vpim_device_info);
void rank_lock(struct dpu_rank_t *rank);
void rank_unlock(struct dpu_rank_t *rank);
int rank_islocked(struct dpu_rank_t *rank);
#endif