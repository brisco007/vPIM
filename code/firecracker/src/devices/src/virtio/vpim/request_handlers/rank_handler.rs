#![allow(nonstandard_style)]
#![allow(unused_imports)]
use crate::virtio::mappings::xeon_sp_mapping::operations::read_from_rank_meta;
use crate::virtio::{dpu_addr_translation::MAX_NR_DPUS_PER_RANK, request_handlers::xfer_types::SZ_4K};

use crate::virtio::vpim::request_handlers::request::*;
use std::result::Result;
use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::vpim::*;
use super::xfer_types::*;
use std::sync::Arc;
use crate::virtio::utilities::types::{U64Ptr, U8Ptr};
use std::ptr::null_mut;
///request for rank configuration
pub const NB_BUFERS_FOR_TRANSFER:usize = 64;

use crate::virtio::vpim::mappings::xeon_sp_mapping::operations::write_to_rank_meta;
use crate::virtio::vpim::mappings::xeon_sp_mapping::operations::write_to_rank_batch_meta;

use utils::eventfd::EventFd;
use std::sync::atomic::Ordering;
use crate::virtio::{IrqTrigger, IrqType};

pub const USE_MULTITHREADING : bool = true;
pub const USE_MULTITHREADING_WRITE : bool = true;

impl VPIMDevice{
    ///This function is used to execute a write operation over the rank. 
    /// Here is another function **handle_read_from_rank_address** which is used to read data from the rank
    pub(crate) fn handle_write_to_rank_address(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
        let xfer_matrix = self.handle_receive_xfer_matrix();
        let xfer_matrix_ref = Box::<dpu_transfer_mram_address>::new(xfer_matrix);
        let _rank = Arc::clone(&self.rank);
        
        if USE_MULTITHREADING == true && USE_MULTITHREADING_WRITE==true {
            let irq_evt = &self.irq_trigger.irq_evt.try_clone().unwrap();
            let output = write_to_rank_meta(_rank, xfer_matrix_ref, self.device_state.mem().unwrap(), irq_evt);
            self.irq_trigger.irq_status.fetch_or(IrqType::Vring as usize, Ordering::SeqCst);
            
        } else {

            let output = self.addr_translation.write_to_rank(_rank, xfer_matrix_ref, self.device_state.mem().unwrap());
            match output {
                Ok(_) => {
                    match self.signal_used_queue() {
                        Ok(_) => {
                        }
                        Err(e) => {
                            println!("firecracker:  here is the error on signaling {:?}", e);
                            return Err(e);
                        }
                    }
                }
                Err(e) => {
                    println!("firecracker:  here is the error on writing {:?}", e);
                    return Result::Err(VPIMDeviceError::ErrorWritingToRank);
                }
            }

        }
        
        /* 
        
        */
        

       Ok(())
    }

    ///This function is used to execute a batch of write operations over the rank. 
    /// Here is another function **handle_read_from_rank_address** which is used to read data from the rank
    pub(crate) fn handle_write_to_rank_address_batch(&mut self, request:Request) -> Result<(), VPIMDeviceError>{
        //println!("[FIRECRACKER] : BATCH SIZE : {}", request.payload_size);
        let mut xfer_matrices = self.handle_receive_xfer_matrix_batch(request.payload_size);
        xfer_matrices.nb_matrices = request.payload_size as u32;
        let xfer_matrices_ref = Box::<dpu_transfer_mram_batch>::new(xfer_matrices);
        let _rank = Arc::clone(&self.rank);

        if USE_MULTITHREADING == true && USE_MULTITHREADING_WRITE==true {
            let irq_evt = &self.irq_trigger.irq_evt.try_clone().unwrap();
            let output = write_to_rank_batch_meta(_rank, xfer_matrices_ref, self.device_state.mem().unwrap(), irq_evt);
            self.irq_trigger.irq_status.fetch_or(IrqType::Vring as usize, Ordering::SeqCst);
            
        } else {
            let output = self.addr_translation.write_to_rank_batch(_rank, xfer_matrices_ref, self.device_state.mem().unwrap());
            match output {
                Ok(_) => {
                    match self.signal_used_queue() {
                        Ok(_) => {
                        }
                        Err(e) => {
                            println!("firecracker:  here is the error on signaling {:?}", e);
                            return Err(e);
                        }
                    }
                }
                Err(e) => {
                    println!("firecracker:  here is the error on writing {:?}", e);
                    return Result::Err(VPIMDeviceError::ErrorWritingToRank);
                }
            }
        }
        
       Ok(())
    }

    ///See **handle_write_to_rank_address**
    pub(crate) fn handle_read_from_rank_address(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
        let xfer_matrix = self.handle_receive_xfer_matrix();
        let xfer_matrix_ref = Box::<dpu_transfer_mram_address>::new(xfer_matrix);
        let _rank = Arc::clone(&self.rank);

        if USE_MULTITHREADING == true {
            let irq_evt = &self.irq_trigger.irq_evt.try_clone().unwrap();
            let output = read_from_rank_meta(_rank, xfer_matrix_ref, self.device_state.mem().unwrap(), irq_evt);
            self.irq_trigger.irq_status.fetch_or(IrqType::Vring as usize, Ordering::SeqCst);
            

        } else {
            let _output = self.addr_translation.read_from_rank(_rank, xfer_matrix_ref, self.device_state.mem().unwrap());
            match self.signal_used_queue() {
                Ok(_) => {                

                }
                Err(e) => {
                    println!("firecracker:  here is the error on signaling {:?}", e);
                    return Err(e);
                }
            }
        }


        
       Ok(())
    }

    
    ///This function receives the matric layout from the guest kernel and sends to the backend
    pub(crate) fn handle_receive_xfer_matrix(&mut self) -> dpu_transfer_mram_address{
        let mut xfer_matrix = dpu_transfer_mram_address {
            ptr: [xfer_page_table {
                nb_pages: 0,
                off_first_page: 0,
                pages: U64Ptr(null_mut())
            }; MAX_NR_DPUS_PER_RANK],
            offset_in_mram: 0,
            size: 0
        };
        let transfer_info = self.next_content::<dpu_transfer_info>().unwrap();
        xfer_matrix.offset_in_mram = transfer_info.offset_in_mram;
        xfer_matrix.size = transfer_info.size;
        for i in 0..MAX_NR_DPUS_PER_RANK{
            let xfer_info = self.next_content::<xfer_info>().unwrap();
            xfer_matrix.ptr[i].nb_pages = xfer_info.nb_pages;
            xfer_matrix.ptr[i].off_first_page = xfer_info.off_first_page as u32;
            xfer_matrix.ptr[i].pages = if xfer_info.nb_pages != 0 { self.next_content_as_array(&xfer_info.nb_pages).unwrap()} else { U64Ptr(null_mut())} ;
        }
        xfer_matrix
    }

        
     ///This function receives the matric layout from the guest kernel and sends to the backend
    pub(crate) fn handle_receive_xfer_matrix_batch(&mut self, batch_size: usize) -> dpu_transfer_mram_batch{
        let mut xfer_matrices = dpu_transfer_mram_batch::default();
        let transfer_info = self.next_content::<dpu_transfer_info_batch>().unwrap();    
        //println!("Transfer info batch {:#?}", transfer_info);
        for i in 0..batch_size {
            xfer_matrices.matrix[i].offset_in_mram = transfer_info.content[i].offset_in_mram;
            xfer_matrices.matrix[i].size = transfer_info.content[i].size;
        }
        
        for i in 0..MAX_NR_DPUS_PER_RANK {
            let mut offset = 0;
            let mut total_pages = 0;
            let xfer_infos = self.next_content::<xfer_info_batch>().unwrap();

            for j in 0..batch_size {
                xfer_matrices.matrix[j].ptr[i].nb_pages = xfer_infos.content[j].nb_pages;
                total_pages += xfer_infos.content[j].nb_pages;
                xfer_matrices.matrix[j].ptr[i].off_first_page = xfer_infos.content[j].off_first_page;
            }
            let data = if total_pages != 0 {self.next_batch().unwrap()} else {U64Ptr(null_mut())};

            for j in 0..batch_size {
                if total_pages!=0 {
                    unsafe {
                        xfer_matrices.matrix[j].ptr[i].pages =  U64Ptr(data.0.add(offset));
                        offset += (xfer_infos.content[j].nb_pages) as usize;
                    }  
                }
                
            }
        }
        return xfer_matrices;
    }
}