// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#![doc(hidden)]

use std::u32;

use crate::virtio::test_utils::VirtQueue;
#[cfg(test)]
use crate::virtio::{
    vpim::NUM_QUEUES, VPIMDevice, IrqType, TRANSFER_QUEUE
};

#[cfg(test)]
pub fn invoke_handler_for_queue_event(b: &mut VPIMDevice, queue_index: usize) {
    use crate::virtio::TRANSFER_QUEUE;

    assert!(queue_index < NUM_QUEUES);
    // Trigger the queue event.
    b.queue_evts[queue_index].write(1).unwrap();
    // Handle event.
    match queue_index {
      TRANSFER_QUEUE => b.process_increment_event().unwrap(),
        _ => unreachable!(),
    };
    // Validate the queue operation finished successfully.
    assert!(b.irq_trigger.has_pending_irq(IrqType::Vring));
}

pub fn set_request(queue: &VirtQueue, idx: usize, addr: u64, len: u32, flags: u16) {
    // Set the index of the next request.
    queue.avail.idx.set((idx + 1) as u16);
    // Set the current descriptor table entry index.
    queue.avail.ring[idx].set(idx as u16);
    // Set the current descriptor table entry.
    queue.dtable[idx].set(addr, len, flags, 1);
}

pub fn check_request_completion(queue: &VirtQueue, idx: usize) {
    // Check that the next used will be idx + 1.
    println!("check_request_completion : queue.used.idx.get() = {}", queue.used.idx.get());
    assert_eq!(queue.used.idx.get(), (idx + 1) as u16);
    // Check that the current used is idx.
    println!("check_request_completion : queue.used.ring = {}", queue.used.ring[0].get().len);
    println!("check_request_completion : queue.used.ring[idx].get().id = {}", queue.used.ring[idx].get().id);
    assert_eq!(queue.used.ring[idx].get().id, idx as u32);
    // The length of the completed request is 0.
    println!("check_request_completion : queue.used.ring[idx].get().len = {}", queue.used.ring[idx].get().len);
    assert_eq!(queue.used.ring[idx].get().len, 0);
}
