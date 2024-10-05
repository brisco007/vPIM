/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/version.h>
#include "dpu_rank.h"
#include "dpu_region_address_translation.h"


enum backend dpu_get_translation_config(struct device *dev,
					unsigned int default_backend){
	return DPU_BANKEND_VIRTIO;
}

void dpu_region_set_address_translation(
	struct dpu_region_address_translation *addr_translate,
	enum backend backend, const struct pci_dev *pci_dev)
{
	struct dpu_region_address_translation *tr;

	switch (backend) {
	case DPU_BANKEND_VIRTIO:
		pr_info("dpu_region: Using virtio config\n");
		tr = &virtio_translate;	
		break;
	default:
		pr_err("dpu_region: Unknown backend\n");
		return;
	}

	memcpy(addr_translate, tr,
	       sizeof(struct dpu_region_address_translation));
}

MODULE_ALIAS("dmi:*:svnUPMEM:*");
