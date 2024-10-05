// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

//! Defines the structures needed for saving/restoring vpim devices.

use std::sync::atomic::AtomicUsize;
use std::sync::Arc;
/* use std::time::Duration;
use timerfd::{SetTimeFlags, TimerState}; */

use snapshot::Persist;
use versionize::{VersionMap, Versionize, VersionizeResult};
use versionize_derive::Versionize;

use vm_memory::GuestMemoryMmap;

use super::*;

use crate::virtio::vpim::device::ConfigSpace;
use crate::virtio::persist::VirtioDeviceState;
use crate::virtio::{DeviceState, TYPE_VPIM};

#[derive(Clone, Versionize)]
// NOTICE: Any changes to this structure require a snapshot version bump.
pub(crate) struct VPIMConfigSpaceState {
    pub hold_rank: u8
}


#[derive(Clone, Versionize)]
// NOTICE: Any changes to this structure require a snapshot version bump.
pub struct VPIMDeviceState {
    config_space: VPIMConfigSpaceState,
    virtio_state: VirtioDeviceState,
}

pub struct VPIMDeviceConstructorArgs {
    pub mem: GuestMemoryMmap,
}

impl Persist<'_> for VPIMDevice {
    type State = VPIMDeviceState;
    type ConstructorArgs = VPIMDeviceConstructorArgs;
    type Error = super::Error;

    fn save(&self) -> Self::State {
        VPIMDeviceState {
            config_space: VPIMConfigSpaceState {
                hold_rank: self.config_space.hold_rank
               /*  base_addr: self.config_space.base_addr,
                size: self.config_space.size,
                mode: self.config_space.mode, */
            },
            virtio_state: VirtioDeviceState::from_device(self),
        }
    }

    fn restore(
        constructor_args: Self::ConstructorArgs,
        state: &Self::State,
    ) -> std::result::Result<Self, Self::Error> {

        let mut vpim = VPIMDevice::new(0)?;

        let num_queues = NUM_QUEUES;
        vpim.queues = state
            .virtio_state
            .build_queues_checked(&constructor_args.mem, TYPE_VPIM, num_queues, QUEUE_SIZE)
            .map_err(|_| Self::Error::QueueRestoreError)?;
        vpim.irq_trigger.irq_status =
            Arc::new(AtomicUsize::new(state.virtio_state.interrupt_status));
        vpim.avail_features = state.virtio_state.avail_features;
        vpim.acked_features = state.virtio_state.acked_features;
        vpim.config_space = ConfigSpace {
            hold_rank: 0,
        };

        if state.virtio_state.activated {
            vpim.device_state = DeviceState::Activated(constructor_args.mem);
        }

        Ok(vpim)
    }
}
