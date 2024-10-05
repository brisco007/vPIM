#![allow(nonstandard_style)]
use crate::virtio::vpim::request_handlers::request::*;
use std::result::Result;
use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::vpim::*;
use vm_memory::{ByteValued, Bytes, GuestAddress};
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct address_info{
    pub address : u64
}
unsafe impl ByteValued for address_info {}


impl VPIMDevice{
    pub(crate) fn handle_address_test(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
        let output  = self.next_content::<address_info>();
        let address;
        match output {
            Err(e) => {
                println!("address_test: here is the error on header {:?}", e);
                return Err(e);
            }
            Ok(info) => {
                println!("The VM sends a phys address: {:?}", info);
                address = info.address;
            }
        };
        let mem = self.device_state.mem().unwrap();
        let mut buf: [u8; 1] = [0];
        let mut i=0;
        while i<=10 {
            let addr = GuestAddress(address+i);
            let _output = mem.read(&mut buf, addr);
            println!("index {}: {:?}", i, buf);
            i=i+1;
        }
        let mut buf = [9, 8, 7, 6, 5, 4, 3, 2, 1];
        let addr = GuestAddress(address);
        let _output = mem.write(&mut buf, addr);
        match self.signal_used_queue() {
            Ok(_) => {}
            Err(e) => {
                println!("firecracker:  here is the error on signaling {:?}", e);
                return Err(e);
            }
        }
        Ok(())
    }
}
