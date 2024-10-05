// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
//This is the file that describe the virtio-pim device as a module
pub mod device;
pub mod event_handler;
pub mod persist;
pub mod test_utils;
pub mod dpu_addr_translation;
pub mod mappings;
pub mod request_handlers;
pub mod utilities;

use vm_memory::GuestMemoryError;

pub use self::device::VPIMDevice;
pub use self::device::VPIMDeviceConfig;
pub use self::event_handler::*;

/// Device ID used in MMIO device identification.
/// Because VPIM is unique per-vm, this ID can be hardcoded.
pub const VPIM_DEV_ID: &str = "dpu_rank";
pub const CONFIG_SPACE_SIZE: usize = 8;
pub const QUEUE_SIZE: u16 = 512;
pub const NUM_QUEUES: usize = 2;
pub const QUEUE_SIZES: &[u16] = &[512,256];

pub const TRANSFER_QUEUE: usize = 0;
pub const CONTROL_QUEUE: usize = 1;
// The feature bitmap for virtio balloon.
const VIRTIO_PIM_F_SAFE_MODE: u64 = 0; // Enable statistics.
const VIRTIO_PIM_F_PERF_MODE: u64 = 1; // Deflate balloon on OOM.

#[derive(Debug)]
pub enum Error {
//TODO : Add more error types from dpu_error.h
    /// Activation error.
    Activate(super::ActivateError),
    /// Guest gave us bad memory addresses.
    GuestMemory(GuestMemoryError),
    /// Device not activated yet.
    DeviceNotActive,
    /// EventFd error.
    EventFd(std::io::Error),
    /// Received error while sending an interrupt.
    InterruptError(std::io::Error),
    /// Guest gave us a malformed descriptor.
    MalformedDescriptor,
    /// Guest gave us a malformed payload.
    MalformedPayload,
    /// Error restoring the device queues.
    QueueRestoreError,
    /// Error while processing the virt queues.
    Queue(super::QueueError),
    /// No PIM device found.
    DeviceNotFound,
    UnsupportedRequest,
    ResponseNotAllocated,
    NoRankAvailable,
    CantConnectWithManager,
    KernelModuleIncompatible,
    ErrorCommitingCommand,
    ErrorGettingCommand,
    ErrorWritingToRank,
    ErrorReadingFromRank,
}

