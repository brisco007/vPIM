// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

use std::os::unix::io::AsRawFd;

use event_manager::{EventOps, Events, MutEventSubscriber};
use logger::{debug, error, warn};
use utils::epoll::EventSet;

use crate::report_vpim_event_fail;
use crate::virtio::{
    vpim::device::VPIMDevice, VirtioDevice, TRANSFER_QUEUE, CONTROL_QUEUE
};

impl VPIMDevice {
    fn register_runtime_events(&self, ops: &mut EventOps) {
        if let Err(e) = ops.add(Events::new(&self.queue_evts[TRANSFER_QUEUE], EventSet::IN)) {
            error!("Failed to register transfer queue event: {}", e);
        }
        if let Err(e) = ops.add(Events::new(&self.queue_evts[CONTROL_QUEUE], EventSet::IN)) {
            error!("Failed to register control queue event: {}", e);
        }
    }

    fn register_activate_event(&self, ops: &mut EventOps) {
        if let Err(e) = ops.add(Events::new(&self.activate_evt, EventSet::IN)) {
            error!("Failed to register activate event: {}", e);
        }
    }

    fn process_activate_event(&self, ops: &mut EventOps) {
        debug!("vpim: activate event");
        if let Err(e) = self.activate_evt.read() {
            error!("Failed to consume vpim activate event: {:?}", e);
        }
        self.register_runtime_events(ops);
        if let Err(e) = ops.remove(Events::new(&self.activate_evt, EventSet::IN)) {
            error!("Failed to un-register activate event: {}", e);
        }
    }
}

impl MutEventSubscriber for VPIMDevice {
    fn process(&mut self, event: Events, ops: &mut EventOps) {
        let source = event.fd();
        let event_set = event.event_set();
        let supported_events = EventSet::IN;

        if !supported_events.contains(event_set) {
            warn!(
                "Received unknown event: {:?} from source: {:?}",
                event_set, source
            );
            return;
        }

        if self.is_activated() {
            let virtq_transfer_ev_fd = self.queue_evts[TRANSFER_QUEUE].as_raw_fd();
            let virtq_control_ev_fd = self.queue_evts[CONTROL_QUEUE].as_raw_fd();
            let activate_fd = self.activate_evt.as_raw_fd();

            // Looks better than C style if/else if/else.
            match source {
                _ if source == virtq_transfer_ev_fd => self
                    .handle_request()
                    .unwrap_or_else(report_vpim_event_fail),
                _ if source == virtq_control_ev_fd => self
                    .handle_ctrl_req()
                    .unwrap_or_else(report_vpim_event_fail),

                _ if activate_fd == source => self.process_activate_event(ops),
                _ => {
                    warn!("vpim: Spurious event received: {:?}", source);
                }
            };
        } else {
            warn!(
                "vPIM: The device is not yet activated. Spurious event received: {:?}",
                source
            );
        }
    }

    fn init(&mut self, ops: &mut EventOps) {
        // This function can be called during different points in the device lifetime:
        //  - shortly after device creation,
        //  - on device activation (is-activated already true at this point),
        //  - on device restore from snapshot.
        if self.is_activated() {
            self.register_runtime_events(ops);
        } else {
            self.register_activate_event(ops);
        }
    }
}

#[cfg(test)]
pub mod tests {
    use std::sync::{Arc, Mutex};

    use super::*;
    use crate::virtio::vpim::test_utils::set_request;
    use crate::virtio::test_utils::{default_mem, VirtQueue};
    use event_manager::{EventManager, SubscriberOps};
    use vm_memory::GuestAddress;

    #[test]
    fn test_event_handler() {
        let mut event_manager = EventManager::new().unwrap();
        let mut vpim = VPIMDevice::new().unwrap();
        let mem = default_mem();
        let infq = VirtQueue::new(GuestAddress(0), &mem, 16);
        vpim.set_queue(TRANSFER_QUEUE, infq.create_queue());

        let vpim = Arc::new(Mutex::new(vpim));
        let _id = event_manager.add_subscriber(vpim.clone());

        // Push a queue event, use the increment queue in this test.
        {
            let addr = 0x100;
            set_request(&infq, 0, addr, 4, 0);
            vpim.lock().unwrap().queue_evts[TRANSFER_QUEUE]
                .write(1)
                .unwrap();
        }

        // EventManager should report no events since vpim has only registered
        // its activation event so far (even though there is also a queue event pending).
        let ev_count = event_manager.run_with_timeout(50).unwrap();
        assert_eq!(ev_count, 0);

        // Manually force a queue event and check it's ignored pre-activation.
        {
            let b = vpim.lock().unwrap();
            // Artificially push event.
            b.queue_evts[TRANSFER_QUEUE].write(1).unwrap();
            // Process the pushed event.
            let ev_count = event_manager.run_with_timeout(50).unwrap();
            // Validate there was no queue operation.
            assert_eq!(ev_count, 0);
            assert_eq!(infq.used.idx.get(), 0);
        }

        // Now activate the device.
        vpim.lock().unwrap().activate(mem.clone()).unwrap();
        // Process the activate event.
        let ev_count = event_manager.run_with_timeout(50).unwrap();
        assert_eq!(ev_count, 1);

        // Handle the previously pushed queue event through EventManager.
        event_manager
            .run_with_timeout(100)
            .expect("Metrics event timeout or error.");
        // Make sure the data queue advanced.
        assert_eq!(infq.used.idx.get(), 1);
    }
}
