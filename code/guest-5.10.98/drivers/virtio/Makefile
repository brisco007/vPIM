# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_VIRTIO) += virtio.o virtio_ring.o
obj-$(CONFIG_VIRTIO_MMIO) += virtio_mmio.o
obj-$(CONFIG_VIRTIO_PCI) += virtio_pci.o
virtio_pci-y := virtio_pci_modern.o virtio_pci_common.o
virtio_pci-$(CONFIG_VIRTIO_PCI_LEGACY) += virtio_pci_legacy.o
obj-$(CONFIG_VIRTIO_BALLOON) += virtio_balloon.o
obj-$(CONFIG_VIRTIO_INPUT) += virtio_input.o
obj-$(CONFIG_VIRTIO_VDPA) += virtio_vdpa.o
obj-$(CONFIG_VIRTIO_MEM) += virtio_mem.o
obj-$(CONFIG_VIRTIO_DMA_SHARED_BUFFER) += virtio_dma_buf.o
#obj-$(CONFIG_VIRTIO_PIM) += virtio_pim/
#TODO obj-$(CONFIG_VIRTIO_PIM) += virtio_pim/     ADD THIS line to compile if only we have the configuration valirable
#obj-y += virtio_pim/dpu_region.o
#obj-y += virtio_pim/
obj-y+= virtio_pim/dpu.o \
		virtio_pim/tests/CI_xfer_test.o \
		virtio_pim/tests/phys_address_test.o \
		virtio_pim/tests/test_meta.o \
		virtio_pim/dpu_config.o \
		virtio_pim/dpu_batch.o \
		virtio_pim/dpu_prefetch.o \
		virtio_pim/dpu_rank_ioctl.o \
		virtio_pim/dpu_rank.o \
		virtio_pim/dpu_rank_sysfs.o \
		virtio_pim/mappings/dpu_virtio_translate.o \
		virtio_pim/dpu_region_address_translation.o 
	
