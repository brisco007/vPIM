#![allow(nonstandard_style)]
use crate::virtio::utilities::hardware::dpu_control_interface_array;
use crate::virtio::vpim::request_handlers::request::*;
use std::result::Result;
use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::vpim::*;
use std::sync::Arc;



impl VPIMDevice{
    ///Handler for virtio event commit commands
    pub(crate) fn handle_commit_command(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
        let output  = self.next_content::<dpu_control_interface_array>();
        match output {
            Err(e) => {
                println!("commit CI : here is the error on header {:?}", e);
                return Err(e);
            }
            Ok(ci) => {
                let _rank = Arc::clone(&self.rank);
                let output = self.addr_translation.write_to_cis(_rank, ci.CIS.as_ptr() );
                match output{
                    Ok(_) => {}
                    Err(e) => {
                        println!("firecracker:  here is the error on commiting new CI {:?}", e);
                        return Result::Err(VPIMDeviceError::ErrorCommitingCommand);
                    }
                }
            }
        };
        match self.signal_used_queue() {
            Ok(_) => {}
            Err(e) => {
                println!("firecracker:  here is the error on signaling {:?}", e);
                return Err(e);
            }
        }
        Ok(())
    }

    pub(crate) fn handle_commit_command_batch(&mut self, request:Request) -> Result<(), VPIMDeviceError>{
        for _i in 0..request.payload_size {
            let output  = self.next_content::<dpu_control_interface_array>();
            match output {
                Err(e) => {
                    println!("commit CI : here is the error on header {:?}", e);
                    return Err(e);
                }
                Ok(ci) => {
                    let _rank = Arc::clone(&self.rank);
                    let output = self.addr_translation.write_to_cis(_rank, ci.CIS.as_ptr() );
                    match output{
                        Ok(_) => {
                        
                        }
                        Err(e) => {
                            println!("firecracker:  here is the error on commiting new CI {:?}", e);
                            return Result::Err(VPIMDeviceError::ErrorCommitingCommand);
                        }
                    }
                }
            };
        }
        match self.signal_used_queue() {
            Ok(_) => {}
            Err(e) => {
                println!("firecracker:  here is the error on signaling {:?}", e);
                return Err(e);
            }
        }
        Ok(())
    }

    ///Handler for virtio update commands
    pub(crate) fn handle_update_command(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
        let new_ci ;
        let mut data :dpu_control_interface_array =  dpu_control_interface_array::default();
        let _rank = Arc::clone(&self.rank);
        let output = self.addr_translation.read_from_cis(_rank, data.CIS.as_mut_ptr() );
        match output {
            Ok(_) => {
                new_ci = data;
            }
            Err(e) => {
                println!("firecracker:  here is the error on reading {:?}", e);
                return Err(VPIMDeviceError::ErrorGettingCommand);
            }
        }
        let result = self.commit_response::<dpu_control_interface_array>(new_ci);

        match self.signal_used_queue() {
            Ok(_) => {}
            Err(e) => {
                println!("firecracker:  here is the error on signaling {:?}", e);
                return Err(e);
            }
        }
        result
    }
}