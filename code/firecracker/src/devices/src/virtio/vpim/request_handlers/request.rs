#![allow(unused_imports)]
#![allow(nonstandard_style)]
use std::time::Instant;

use vm_memory::{ByteValued, Bytes, GuestMemory};

use crate::virtio::utilities::types::{U64Ptr, U8Ptr};
use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::vpim::*;

pub const REQ_CONFIG: i32 = 0;
pub const REQ_WRITE_TO_RANK: i32 = 1;
pub const REQ_READ_FROM_RANK: i32 = 2;
pub const REQ_COMMIT_COMMAND: i32 = 3;
pub const REQ_UPDATE_COMMAND: i32 = 4;
pub const REQ_REALLOC_CONFIG: i32 = 5;
pub const REQ_FREE_CONFIG: i32 = 6;
pub const REQ_COMMIT_COMMAND_BATCH: i32 = 7;
pub const REQ_WRITE_TO_RANK_BATCH: i32 = 8;
pub const REQ_ADDRESS_TEST: i32 = 10;
pub const IS_OWNED: u8 =1;
pub const IS_NOT_OWNED:u8 =0;


#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct Request {
    pub request_type : i32,
    pub(crate) payload_size : usize
}

#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct Response{
    pub response_type : i32,
    pub payload_size : usize
}


unsafe impl ByteValued for Request {}
unsafe impl ByteValued for Response {}


impl VPIMDevice{
    
    pub(crate) fn handle_ctrl_req(&mut self) -> Result<(), VPIMDeviceError> {
        self.queue_evts[CONTROL_QUEUE]
        .read()
        .map_err(VPIMDeviceError::EventFd)?;
        let result: Result<(), VPIMDeviceError>;
        let output  = self.next_content_ctrl::<Request>();

        match output {
            Err(e) =>{
                result = Err(e);
            }
            Ok(content)=>{
                
                match content.request_type{
                    REQ_REALLOC_CONFIG => {
                        result = self.handle_rank_owned()},
                    REQ_FREE_CONFIG => {
                        result = self.handle_rank_not_owned();
                    }
                    _ => return Err(VPIMDeviceError::UnsupportedRequest)
                }
            }
        };
        match result {
            Err(e) =>{
                println!("Error: we have : {:?}\n", e);
            }
            Ok(_header) => {}
        }
        Ok(())
    }

     pub(crate) fn handle_request(&mut self) -> Result<(), VPIMDeviceError> {
        self.queue_evts[TRANSFER_QUEUE]
        .read()
        .map_err(VPIMDeviceError::EventFd)?;
        let result: Result<(), VPIMDeviceError>;
        let output  = self.next_content::<Request>();
        match output {
            Err(e) =>{
                result = Err(e);
            }
            Ok(content)=>{
                
                match content.request_type{
                    REQ_CONFIG => result = self.handle_config(content),
                    REQ_WRITE_TO_RANK => {
                        //println!("[FIRECRACKER] starting  write request handling rank {} : ,time: {:#?}", self.get_id(), Instant::now());
                        
                        result = self.handle_write_to_rank_address(content);
                        //println!("[FIRECRACKER] end write request handling rank {} , time : {:#?}", self.get_id(),Instant::now())

                    } ,
                    REQ_WRITE_TO_RANK_BATCH => {
                        //println!("[FIRECRACKER] starting  write batch request handling rank {} , time : {:#?}", self.get_id(), Instant::now());

                        result = self.handle_write_to_rank_address_batch(content);
                        //println!("[FIRECRACKER] end wriet batch request handling rank {} , time : {:#?}", self.get_id(), Instant::now())

                    }
                    REQ_READ_FROM_RANK => {
                        //println!("[FIRECRACKER] starting  read request handling rank {} , time : {:#?}", self.get_id(), Instant::now());

                        result = self.handle_read_from_rank_address(content);
                        //println!("[FIRECRACKER] end read request handling rank {} , time : {:#?}", self.get_id(), Instant::now())

                    } ,
                    REQ_COMMIT_COMMAND => {
                        result = self.handle_commit_command(content)
                    },
                    REQ_COMMIT_COMMAND_BATCH => {
                        result = self.handle_commit_command_batch(content)
                    },
                    REQ_UPDATE_COMMAND => {
                        result = self.handle_update_command(content)},
                    REQ_ADDRESS_TEST => result = self.handle_address_test(content),
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

        Ok(())
    }

    
    pub(crate) fn next_content<T:ByteValued>(&mut self) -> Result<T, VPIMDeviceError>{
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[TRANSFER_QUEUE];
        //pop four descriptors from the queue if Some
        let desc = queue.pop(mem);
        if desc.is_none() {
            println!("Err: The received buffer is empty");
            return Err(VPIMDeviceError::MalformedDescriptor);
        }
        let descriptor = desc.unwrap();
        let header: Result<T, GuestMemoryError> = mem.read_obj(descriptor.addr);
        let _result = queue.add_used(mem,  descriptor.index, descriptor.len);
        match header {
            Ok(header) => {
                return Ok(header);
            }
            Err(e) => {
                return Err(VPIMDeviceError::GuestMemory(e));
            }
        }
    }

    pub(crate) fn next_content_ctrl<T:ByteValued>(&mut self) -> Result<T, VPIMDeviceError>{
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[CONTROL_QUEUE];
        //pop four descriptors from the queue if Some
        let desc = queue.pop(mem);
        if desc.is_none() {
            println!("Err: The received buffer is empty");
            return Err(VPIMDeviceError::MalformedDescriptor);
        }
        let descriptor = desc.unwrap();
        let header: Result<T, GuestMemoryError> = mem.read_obj(descriptor.addr);
        let _result = queue.add_used(mem,  descriptor.index, descriptor.len);
        match header {
            Ok(header) => {
                return Ok(header);
            }
            Err(e) => {
                return Err(VPIMDeviceError::GuestMemory(e));
            }
        }
    }

    /// This function reads the arrays of addresses in the virtio buffer for the transfer matrix
    pub(crate) fn next_content_as_array(&mut self, _nb_pages: &u64) -> Result<U64Ptr, VPIMDeviceError>{
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[TRANSFER_QUEUE];
        //pop four descriptors from the queue if Some
        let desc = queue.pop(mem);
        if desc.is_none() {
            println!("Err: The received buffer is empty");
            return Err(VPIMDeviceError::MalformedDescriptor);
        }
        let descriptor = desc.unwrap();
        let myArray = mem.get_slice(descriptor.addr, descriptor.len as usize);
        let _result = queue.add_used(mem,  descriptor.index, descriptor.len);
        match myArray {
            Ok(data) => {
                return Ok(U64Ptr(data.as_ptr() as *mut u64));
            }
            Err(e) => {
                return Err(VPIMDeviceError::GuestMemory(e));
            }
        }
    }
   /// This function reads the arrays of addresses in the virtio buffer for the transfer matrix
   pub(crate) fn next_batch(&mut self) -> Result<U64Ptr, VPIMDeviceError>{
       // println!("[FIRECRACKER] STARTED NEXT BATCH");
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[TRANSFER_QUEUE];
        //pop four descriptors from the queue if Some
        let desc = queue.pop(mem);
        if desc.is_none() {
            println!("Err: The received buffer is empty");
            return Err(VPIMDeviceError::MalformedDescriptor);
        }
        let descriptor = desc.unwrap();
        let myArray = mem.get_slice(descriptor.addr, descriptor.len as usize);
        let _result = queue.add_used(mem,  descriptor.index, descriptor.len);
        match myArray {
            Ok(data) => {
                //println!("[FIRECRACKER] END NEXT BATCH");

                return Ok(U8Ptr(data.as_ptr()).toU64Ptr());
            }
            Err(e) => {
                return Err(VPIMDeviceError::GuestMemory(e));
            }
        }

    }
    pub(crate) fn commit_response<T:ByteValued>(&mut self, response:T)-> Result<(), VPIMDeviceError>{
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[TRANSFER_QUEUE];
        let desc = queue.pop(mem);
        if desc.is_none() {
            return Err(VPIMDeviceError::ResponseNotAllocated);
        }
        let response_desc = desc.unwrap();
        queue.add_used(mem,  response_desc.index, response_desc.len).map_err(VPIMDeviceError::Queue)?;
        match mem.write_obj(response,response_desc.addr){
            Ok(_) =>{}
            Err(e) => {
                return Err(Error::GuestMemory(e));
            }
        } 
        Ok(())
    }

    pub(crate) fn commit_response_ctrl<T:ByteValued>(&mut self, response:T)-> Result<(), VPIMDeviceError>{
        let mem = self.device_state.mem().unwrap();
        let queue = &mut self.queues[CONTROL_QUEUE];
        let desc = queue.pop(mem);
        if desc.is_none() {
            return Err(VPIMDeviceError::ResponseNotAllocated);
        }
        let response_desc = desc.unwrap();
        queue.add_used(mem,  response_desc.index, response_desc.len).map_err(VPIMDeviceError::Queue)?;
        match mem.write_obj(response,response_desc.addr){
            Ok(_) =>{}
            Err(e) => {
                return Err(Error::GuestMemory(e));
            }
        } 
        Ok(())
    }
}



