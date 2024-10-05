// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#![allow(nonstandard_style)]
#![allow(dead_code)]
#![allow(unused_imports)]
#![allow(unused_assignments)]
#![allow(unused_variables)]
#![allow(improper_ctypes)]
use serde::{Serialize, Deserialize};
use std::cmp;
use std::io::Write;
use std::result::Result;
use std::sync::atomic::AtomicUsize;
use std::sync::mpsc::{Sender, Receiver, channel};
use std::sync::{Arc, Mutex, };
use std::thread::JoinHandle;
use ::logger::{error, IncMetric, METRICS};
use ::utils::eventfd::EventFd;
use ::virtio_gen::virtio_blk::*;
use ::vm_memory::{ ByteValued, GuestMemoryMmap};

use super::*;
use super::request_handlers::request::{IS_OWNED, IS_NOT_OWNED};
use super::utilities::types::dpu_rank_t;
use super::utilities::manager::{Manager, Request, REQ_ALLOC, RES_OK, RES_FAILED};
use super::super::{ActivateResult, DeviceState, Queue, VirtioDevice, TYPE_VPIM};

use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::{IrqTrigger, IrqType};
use crate::virtio::vpim::utilities::hardware::{dpu_rank_fs, dpu_description_t};
use crate::virtio::vpim::utilities::dpu_sysfs::*;
use crate::virtio::vpim::dpu_addr_translation::*;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct ConfigSpace {
    pub hold_rank: u8
}
pub const TIMEOUT_DELAY:u32 = 90; //in total

/* pub static SENDER: [Arc<Mutex<Sender<i32>>>; 8] = [
    Arc::new(Mutex::new(Sender::new()));8
    // Répétez pour les 6 autres instances
];

pub static RECEIVER: [Arc<Mutex<Receiver<i32>>>; 8] = [
    Arc::new(Mutex::new(Receiver::new()));8
    // Répétez pour les 6 autres instances
]; */
// Safe because ConfigSpace only contains plain data.
unsafe impl ByteValued for ConfigSpace {}

// VPIMDeviceStats holds statistics returned from the stats_queue.
#[derive(Clone, Default, Debug, PartialEq)]
pub struct VPIMDeviceConfig {
    pub hold_rank:u8
}
//TODO REMOVE SIZE AND BASE ADDRESS FROM CONFIG FOR THEY ARE USELESS. VM_IN SHALL GO ALSO SINCE IT IS NOT THE PURPOSE OF THE DEVICE
// Virtio VPIMDevice device.
pub struct VPIMDevice {
    // Virtio fields.
    pub(crate) avail_features: u64,
    pub(crate) acked_features: u64,
    pub(crate) config_space: ConfigSpace,
    pub(crate) activate_evt: EventFd,

    // Transport related fields.
    pub(crate) queues: Vec<Queue>,
    pub(crate) queue_evts: [EventFd; NUM_QUEUES],
    pub(crate) device_state: DeviceState,
    pub(crate) irq_trigger: IrqTrigger,

    // Implementation specific fields.
    pub(crate) rank_fs: dpu_rank_fs,
    pub(crate) rank: Arc<Mutex<dpu_rank_t>>,
    pub(crate) description: dpu_description_t,
    pub(crate) is_owner: u8,
    pub(crate) capability_mode: u64,
    pub(crate) addr_translation: DpuRegionAddressTranslation,
    pub(crate) manager: Manager,

}



pub fn is_kernel_module_compatible() -> bool {
    //TODO fill it
    true
}

//build a function that returns the dpu_addr_translation_t struct
fn get_dpu_addr_struct() -> DpuRegionAddressTranslation {

    DpuRegionAddressTranslation::default()    
}

impl VPIMDevice {
    pub fn new(
        hold_rank:u8
    ) -> Result<VPIMDevice, VPIMDeviceError> {
        //SETTING FEATURES AND QUEUES 
        let mut avail_features = 1u64 << VIRTIO_F_VERSION_1;
        
            avail_features |= 1u64 << VIRTIO_PIM_F_SAFE_MODE;
        
            avail_features |= 1u64 << VIRTIO_PIM_F_PERF_MODE;
        
        let queue_evts = [
            EventFd::new(libc::EFD_NONBLOCK).map_err(VPIMDeviceError::EventFd)?,
            EventFd::new(libc::EFD_NONBLOCK).map_err(VPIMDeviceError::EventFd)?
        ];
        let queues: Vec<Queue> = QUEUE_SIZES.iter().map(|&s| Queue::new(s)).collect();

        //VERIFICATION OF THE KERNEL COMPATIBILITY AND RANK ALLOCATION 
        //1 make sure the sdk is compatible to the kernel module
        if !is_kernel_module_compatible() {
            return Err(VPIMDeviceError::KernelModuleIncompatible);
        }

        let mgr = Manager::new();
        if mgr.is_err() {
            return Err(VPIMDeviceError::CantConnectWithManager);
        }
        let mut man = mgr.unwrap();
        let rank_request: Request = Request {
            req_type: REQ_ALLOC,
            req_len: 0,
            vpim_id: man.vpim_id,
        };
        let mut timeout = TIMEOUT_DELAY;
        let mut found = false;
        let mut res = None;
        while timeout > 0 && found == false {
            res = man.send_req(&rank_request);
            if res.is_some() && res.as_ref().unwrap().status == RES_OK {
                found = true;
                //set the vpim id attributed by the manager
                man.vpim_id = res.as_ref().unwrap().vpim_id;
                break;
            } else if res.is_some() && res.as_ref().unwrap().status == RES_FAILED {
                timeout = 0;
                break;
            } else {
                timeout -=1;
            }
           
        }
        if found == false {
            return Err(VPIMDeviceError::NoRankAvailable);
        }

        let rank_fs_opt = dpu_sysfs_try_to_allocate_rank(res.unwrap());
        //get the available rank
        
        if rank_fs_opt.is_none() {
            return Err(VPIMDeviceError::NoRankAvailable);
        }
        let rank_fs = rank_fs_opt.unwrap();

        let mut dpu_addr_translation = get_dpu_addr_struct();
        dpu_addr_translation.one_read = false;
        let id = rank_fs.rank_id;

/*         let (tx, rx) = channel();
        let sender = Arc::new(Mutex::new(tx));
        let receiver = Arc::new(Mutex::new(rx));
        unsafe {
            SENDER[id as usize] = sender;
            RECEIVER[as usize] = receiver;
        } */
        Ok(VPIMDevice {
            avail_features,
            acked_features: 0u64,
            config_space: ConfigSpace {
                hold_rank,
            },
            queue_evts,
            queues,
            irq_trigger: IrqTrigger::new().map_err(VPIMDeviceError::EventFd)?,
            device_state: DeviceState::Inactive,
            activate_evt: EventFd::new(libc::EFD_NONBLOCK).map_err(VPIMDeviceError::EventFd)?,
            rank_fs,
            is_owner: IS_OWNED,
            description: dpu_description_t::default(),
            capability_mode: u64::from_str_radix("0x3".trim_start_matches("0x"), 16).unwrap(),
            addr_translation: dpu_addr_translation,
            rank: init_rank_struct(id),
            manager:man,
        })
      
       
       
    }

 /*     pub fn send_event(&self, event: i32) {
        //1 is there is a request and 2 is should terminate
        unsafe {
            let a = SENDER[s.get_id() as usize];
            a.lock().unwrap().send(event);
        }
    } */
 
    pub(crate) fn signal_used_queue(&self) -> Result<(), VPIMDeviceError> {
        self.irq_trigger.trigger_irq(IrqType::Vring).map_err(|e| {
            METRICS.vpim.event_fails.inc();
            VPIMDeviceError::InterruptError(e)
        })
    }

    /// Process device virtio queue(s).
    pub fn process_virtio_queues(&mut self) {
        //TODO fill this if needed
    }

    pub fn set_owned(&mut self){
        self.is_owner = IS_OWNED;
    }
    pub fn set_not_owned(&mut self){
        self.is_owner = IS_NOT_OWNED;
    }

    pub fn check_is_owned(&mut self) -> bool {
        return self.is_owner == IS_OWNED;
    }
    pub fn id(&mut self, _id:u16) -> String {
        VPIM_DEV_ID.to_owned() + self.dpu_sysfs_get_rank_id().to_string().as_str()
    }
    pub fn get_id(&mut self) -> u8 {
        self.rank.lock().unwrap().rank_id
    }
   
    pub fn unmap(&mut self) {
        let rank_clone = Arc::clone(&self.rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();
        
        self.rank_fs.fdpu_dax = None;
    }
    pub fn realloc(&mut self) -> Result<(),VPIMDeviceError>{
        let rank_request: Request = Request {
            req_type: REQ_ALLOC,
            req_len: 0,
            vpim_id: self.manager.vpim_id,
        };
        let mut timeout = TIMEOUT_DELAY;
        let mut found = false;
        let mut res = None;
        while timeout >0 && found == false {
            res = self.manager.send_req(&rank_request);
            if res.is_some() && res.as_ref().unwrap().status == RES_OK {
                found = true;
                //set the vpim id attributed by the manager
                break;
            } else if res.is_some() && res.as_ref().unwrap().status == RES_FAILED {
                timeout = 0;
            }
            timeout -=1;
        }
        if found == false {
            return Err(VPIMDeviceError::NoRankAvailable);
        }

        let rank_fs_opt = dpu_sysfs_try_to_allocate_rank(res.unwrap());
        
        if rank_fs_opt.is_none() {
            return Err(VPIMDeviceError::NoRankAvailable);
        }
        let rank_fs = rank_fs_opt.unwrap();
        self.rank_fs = rank_fs;
        self.config();
        Ok(())
    }
    pub fn config(&mut self) -> VPIMDeviceConfig {
        //acquire the mutex on rank
        let rank_clone = Arc::clone(&self.rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();

        _rank.config_data.is_owned = self.dpu_sysfs_get_is_owned();
        _rank.config_data.usage_count = self.dpu_sysfs_get_usage_count();
        _rank.config_data.rank_count = self.dpu_sysfs_get_rank_count(); //TODO this gives the number of ranks on the dimm, set it to one when allocated a single rank.
        _rank.config_data.part_number = self.dpu_sysfs_get_part_number();
        _rank.config_data.dimm_sn = self.dpu_sysfs_get_dimm_serial_number();
        _rank.config_data.mcu_version = self.dpu_sysfs_get_mcu_version();
        _rank.config_data.channel_id = self.dpu_sysfs_get_channel_id();
        _rank.config_data.mode = 2; //TODO force the safe mode for the moment.
        _rank.config_data.dpu_chip_id = self.dpu_sysfs_get_dpu_chip_id();
        _rank.config_data.debug_mode = self.dpu_sysfs_get_debug_mode();
        _rank.config_data.capabilities = self.dpu_sysfs_get_capabilities();
        _rank.config_data.rank_id = self.dpu_sysfs_get_rank_id();
        _rank.config_data.rank_index = self.dpu_sysfs_get_rank_index();
        _rank.set_rank_id(self.rank_fs.rank_id);

        _rank.set_numa_node(self.dpu_sysfs_get_numa_node());
        _rank.set_channel_id(self.dpu_sysfs_get_channel_id());
        _rank.set_dpu_chip_id(self.dpu_sysfs_get_dpu_chip_id());

        self.dpu_sysfs_get_hardware_description();
        
        _rank.set_description(self.description);
        _rank.set_nb_cis(_rank.description.hw.topology.nr_of_control_interfaces);
        self.byte_order_parse(_rank);
        _rank.set_region_size(self.dpu_sysfs_get_region_size());
       
        _rank.do_mmap(&self.rank_fs);
        
        //set interleaving infos
        match self.addr_translation.init_rank(_rank) {
            Ok(_i) => {
            }
            Err(e) => {
                println!("VPIMDevice::config : here is the error on rank {:?} ; Couldn't inintialize", e);
            }
        };

 
      /*   let receiver: Arc<Mutex<Receiver<i32>>> = self.receiver.clone();

        let req_handler = self.spawn_thread(receiver);
        self.req_handler = Some(req_handler);  */
        VPIMDeviceConfig {
          hold_rank:self.config_space.hold_rank
        }

    }


}

/* pub fn spawn_thread(dev : Arc<Mutex<VPIMDevice>>/* , receiver: Arc<Mutex<Receiver<i32>>> */)/* -> JoinHandle<Result<(), Error>> */{
    let self_clone = dev.clone();
    let res = std::thread::spawn(move || {  
        let mut s = self_clone.lock().unwrap();
        let mut receiver = RECEIVER[s.get_id() as usize].lock().unwrap();
        println!("hahahahahahahaha");
        // Boucle infinie pour recevoir et traiter les événements
        loop {
            // Attendre un événement
            let event = receiver.recv().unwrap();
            if event == 1 {
                println!("[FIRECRACKER] Thread received event: {}", event);
                let result: Result<(), VPIMDeviceError>;
                let output  = s.next_content::<frontend::request::Request>();
                match output {
                    Err(e) =>{
                        result = Err(e);
                    }
                    Ok(content)=>{
                        
                        match content.request_type{
                            REQ_CONFIG => result = s.handle_config(content),
                            REQ_WRITE_TO_RANK => {
                                println!("[FIRECRACKER] starting  write request handling rank {}", s.get_id());
                                result = s.handle_write_to_rank_address(content);
                                println!("[FIRECRACKER] end write request handling rank {}", s.get_id())

                            } ,
                            REQ_READ_FROM_RANK => {
                                println!("[FIRECRACKER] starting  read request handling rank {}", s.get_id());

                                result = s.handle_read_from_rank_address(content);
                                println!("[FIRECRACKER] end read request handling rank {}", s.get_id())

                            } ,
                            REQ_COMMIT_COMMAND => {
                                result = s.handle_commit_command(content)
                            },
                            REQ_UPDATE_COMMAND => {
                                result = s.handle_update_command(content)},
                            REQ_ADDRESS_TEST => result = s.handle_address_test(content),
                            _ => return Err(VPIMDeviceError::UnsupportedRequest)
                        }
                    }
                };
                match result {
                    Err(e) =>{
                        println!("Error: {:?}\n", e);
                    }
                    Ok(_header) => {}
                }






            } else if event == 2 {
                println!("[FIRECRACKER] Break: {}", event);
                break;
            } else {
                println!("[FIRECRACKER] unsupported event {}", event);
            }

        }
        Ok(())
    });

    //dev.clone().lock().unwrap().req_handler = Some(res);

}
 */
/*  impl Drop for VPIMDevice {
    fn drop(&mut self) {
        self.send_event(2);
        //self.req_handler.join().unwrap()
    }
}  */
impl VirtioDevice for VPIMDevice {
    fn device_type(&self) -> u32 {
        TYPE_VPIM
    }

    fn queues(&self) -> &[Queue] {
        &self.queues
    }
    
    fn queues_mut(&mut self) -> &mut [Queue] {
        &mut self.queues
    }

    fn queue_events(&self) -> &[EventFd] {
        &self.queue_evts
    }

    fn interrupt_evt(&self) -> &EventFd {
        &self.irq_trigger.irq_evt
    }

    fn interrupt_status(&self) -> Arc<AtomicUsize> {
        self.irq_trigger.irq_status.clone()
    }


    fn avail_features(&self) -> u64 {
        self.avail_features
    }

    fn acked_features(&self) -> u64 {
        self.acked_features
    }

    fn set_acked_features(&mut self, acked_features: u64) {
        self.acked_features = acked_features;
    }

    fn read_config(&self, offset: u64, mut data: &mut [u8]) {
        let config_space_bytes = self.config_space.as_slice();
        let config_len = config_space_bytes.len() as u64;
        if offset >= config_len {
            error!("Failed to read config space");
            return;
        }

        if let Some(end) = offset.checked_add(data.len() as u64) {
            // This write can't fail, offset and end are checked against config_len.
            data.write_all(
                &config_space_bytes[offset as usize..cmp::min(end, config_len) as usize],
            )
            .unwrap();
        }
    }

    fn write_config(&mut self, offset: u64, data: &[u8]) {
        let data_len = data.len() as u64;
        let config_space_bytes = self.config_space.as_mut_slice();
        let config_len = config_space_bytes.len() as u64;
        if offset + data_len > config_len {
            error!("Failed to write config space");
            return;
        }
        config_space_bytes[offset as usize..(offset + data_len) as usize].copy_from_slice(data);
    }

    fn is_activated(&self) -> bool {
        self.device_state.is_activated()
    }

    fn activate(&mut self, mem: GuestMemoryMmap) -> ActivateResult {
        self.device_state = DeviceState::Activated(mem);
        if self.activate_evt.write(1).is_err() {
            error!("VPIMDevice: Cannot write to activate_evt");
            METRICS.vpim.activate_fails.inc();
            self.device_state = DeviceState::Inactive;
            return Err(super::super::ActivateError::BadActivate);
        }

        Ok(())
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use std::u32;
    use super::*;
    use crate::virtio::vpim::test_utils::{
        check_request_completion, invoke_handler_for_queue_event, set_request,
    };
    use crate::virtio::test_utils::{default_mem, VirtQueue};
    use crate::virtio::{VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE};
    const SIZE_OF_U32: usize = std::mem::size_of::<u32>();
    use vm_memory::GuestAddress;
    impl VPIMDevice {
        pub(crate) fn set_queue(&mut self, idx: usize, q: Queue) {
            self.queues[idx] = q;
        }
    }

    #[test]
    fn test_virtio_pim_rank_init() {
        let mut vpim = VPIMDevice::new(0
        ).unwrap();
        vpim.config();
        let _rank = vpim.rank.lock().unwrap();
        assert_eq!(_rank.rank_id, 1);
        assert_eq!(_rank.numa_node, 0);
        assert_eq!(_rank.channel_id, 4);
        assert_eq!(_rank.dpu_chip_id, 4);
        assert_eq!(_rank.description.hw.topology.nr_of_dpus_per_control_interface, 8);
        assert_eq!(_rank.description.hw.topology.nr_of_control_interfaces, 8);
        assert_eq!(_rank.description.hw.memories.mram_size, 67108864);
        assert_eq!(_rank.description.hw.timings.clock_division, 2);
        assert_eq!(_rank.description.hw.timings.fck_frequency_in_mhz, 700);
        assert_eq!(_rank.nb_cis, 8);
        assert_eq!(_rank.region_size, 8589934592);
        assert_eq!(_rank.runtime.control_interface.slice_info[0].byte_order, 0x000103ff0f8fcfef)

        //If it is okay, then go into the requests. tests the requests handlers. 
    }

    #[test]
    fn test_invalid_request() {
        let mut vpim = VPIMDevice::new(0
        ).unwrap();

        let mem = default_mem();
        // Only initialize the increment queue to demonstrate invalid request handling.
        let infq = VirtQueue::new(GuestAddress(0), &mem, 16);
        vpim.set_queue(TRANSFER_QUEUE, infq.create_queue());
        vpim.activate(mem.clone()).unwrap();

        // Fill the second page with non-zero bytes.
        for i in 0..0x1000 {
            assert!(mem.write_obj::<u32>(45, GuestAddress((1 << 12) + i)).is_ok());
        }

        // Will write the page frame number of the affected frame at this
        // arbitrary address in memory.
        let page_addr = 0x12; //18 in decimal
        //create a new TestStruct variable with a name of test_struct
        println!("eee: queue.used.idx.get() = {}", infq.used.idx.get());
        
        //print the avai_queue of vpim queue
        // Invalid case: the descriptor is write-only.
        {
            mem.write_obj::<u32>(15, GuestAddress(page_addr)).unwrap();
            let mut value = mem.read_obj::<u32>(GuestAddress(page_addr)).unwrap();
            println!("THE NUMBER BEFORE UPDATE : {:?}", value);
            set_request(
                &infq,
                0,
                page_addr,
                SIZE_OF_U32 as u32,
                VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE,
            );
            invoke_handler_for_queue_event(&mut vpim, TRANSFER_QUEUE);  
            println!("eee: queue.used.idx.get() = {}", infq.used.idx.get());
           
            value = mem.read_obj::<u32>(GuestAddress(page_addr)).unwrap();
            println!("THE NUMBER AFTER UPDATE : {:?}", value);
            check_request_completion(&infq, 0);
            // Check that the page was not zeroed.
            for i in 0..0x1000 {
                assert_eq!(mem.read_obj::<u8>(GuestAddress((1 << 12) + i)).unwrap(), 1);
            }
        }

        // Invalid case: descriptor len is not a multiple of 'SIZE_OF_U32'.
        {
            mem.write_obj::<u32>(0x1, GuestAddress(page_addr)).unwrap();
            set_request(
                &infq,
                1,
                page_addr,
                SIZE_OF_U32 as u32 + 1,
                VIRTQ_DESC_F_NEXT,
            );

            invoke_handler_for_queue_event(&mut vpim, TRANSFER_QUEUE);
            check_request_completion(&infq, 1);

            // Check that the page was not zeroed.
            for i in 0..0x1000 {
                assert_eq!(mem.read_obj::<u8>(GuestAddress((1 << 12) + i)).unwrap(), 1);
            }
        }
    }

    #[test]
    fn test_process_vpim_queues() {
        let mut vpim = VPIMDevice::new(0
        ).unwrap();
        let mem = default_mem();
        vpim.activate(mem).unwrap();
        vpim.process_virtio_queues()
    }

/*     #[test]
    fn test_mode() {
        let mut vpim = VPIMDevice::new(
        ).unwrap();
      
        // Switch the state to active.
        vpim.device_state = DeviceState::Activated(
            vm_memory::test_utils::create_guest_memory_unguarded(
                &[(GuestAddress(0x0), 0x1)],
                false,
            )
            .unwrap(),
        );

        assert_eq!(vpim.mode(), VIRTIO_PIM_F_SAFE_MODE);
        assert_eq!(vpim.size_mb(), 8);

        // Update fields through the API.
        let _ = vpim.update_mode(VIRTIO_PIM_F_PERF_MODE);
        assert_eq!(vpim.mode(), VIRTIO_PIM_F_PERF_MODE);
      
    }
 */}
