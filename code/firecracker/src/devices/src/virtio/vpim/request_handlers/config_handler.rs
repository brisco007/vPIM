#![allow(nonstandard_style)]
use crate::virtio::utilities::hardware::*;
use crate::virtio::utilities::types::dpu_rank_request_props;
use crate::virtio::utilities::vpd::dpu_vpd;
use crate::virtio::vpim::request_handlers::request::*;
use std::result::Result;
use crate::virtio::vpim::Error as VPIMDeviceError;
use crate::virtio::vpim::*;
use std::sync::Arc;


impl VPIMDevice{
    pub(crate) fn get_runtime_config(&mut self) -> dpu_runtime_state_t{
        let _rank = self.rank.lock().unwrap();
        let mut runtime = dpu_runtime_state_t::default();
        runtime.run_context.nb_dpu_running = 64;
        for index in 0..8{
            runtime.control_interface.slice_info[index].byte_order = 
            _rank.runtime.control_interface.slice_info[index].byte_order;
        }
        runtime.control_interface.fault_decode = _rank.runtime.control_interface.fault_decode; 
        runtime = _rank.runtime;
        runtime
    }
    
    pub(crate) fn get_bit_config(&mut self) -> dpu_bit_config{
        //FIXME : Should we really leave it empty? do we really need it?
        dpu_bit_config::default()
    }
    
    pub(crate) fn get_dpu_hw(&mut self) -> dpu_hw_description_t{
        let hw;
        let _rank = Arc::new(self.rank.clone());
        hw = _rank.lock().unwrap().description.hw;
        hw
    }
    pub(crate) fn get_other_config_data(&mut self) -> dpu_rank_request_props{
        let data;
        let _rank = Arc::new(self.rank.clone());
        data = _rank.lock().unwrap().config_data;
        data
    }

    pub(crate) fn get_vpd(&mut self) -> dpu_vpd{
        self.dpu_sysfs_initvpd().unwrap()
    }


    ///This function is used to unlink the vUPMEM device to the real rank
    pub(crate) fn handle_rank_not_owned(&mut self) -> Result<(), VPIMDeviceError>{
        match self.signal_used_queue() {
            Ok(_) => {
                if self.config_space.hold_rank == 0 && self.check_is_owned() {
                    self.rank_fs.fdpu_rank = None;
                    self.unmap();
                    self.set_not_owned();
                }
                Ok(())
            }
            Err(e) => {
                println!("firecracker:  here is the error on signaling {:?}", e);
                return Err(e);
            }
        }
    }
    ///This function reallocates the rank to the vUPMEM device
    pub(crate) fn handle_rank_owned(&mut self) -> Result<(), VPIMDeviceError>{
        if self.config_space.hold_rank == 0 && !self.check_is_owned() {
            match self.realloc() {
                Ok(_) => {
                    self.set_owned();
                },
                Err(e) => {
                    return Err(e);
                }
            }
        }
        let runtime = self.get_runtime_config();
        let bit_config = self.get_bit_config();
        let hardware = self.get_dpu_hw();
        let config_data = self.get_other_config_data();
        let vpd = self.get_vpd();

        let mut result = self.commit_response_ctrl::<dpu_runtime_state_t>(runtime);

        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response_ctrl::<dpu_bit_config>(bit_config);
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response_ctrl::<dpu_hw_description_t>(hardware);
        match result {
            Ok(_) =>{
                //println!("### Hardware Config Request: hw description struct sent ");
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response_ctrl::<dpu_rank_request_props>(config_data);
        match result {
            Ok(_) =>{
                //println!("### Hardware Config Request: other config data struct sent ");
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response_ctrl::<dpu_vpd>(vpd);
        match result {
            Ok(_) =>{
                //println!("### Hardware Config Request: vpd_header data struct sent ");
            }
            Err(e) => {
                return Err(e);
            }
        }

        match self.signal_used_queue() {
            Ok(_) => {
            }
            Err(e) => {
                return Err(e);
            }
        }
        result
    }

    ///This handler replies to the request of configuration
    pub(crate) fn handle_config(&mut self, _request:Request) -> Result<(), VPIMDeviceError>{
 
        let runtime = self.get_runtime_config();
        let bit_config = self.get_bit_config();
        let hardware = self.get_dpu_hw();
        let config_data = self.get_other_config_data();
        let vpd = self.get_vpd();
        let mut result = self.commit_response::<dpu_runtime_state_t>(runtime);
        
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response::<dpu_bit_config>(bit_config);
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response::<dpu_hw_description_t>(hardware);
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response::<dpu_rank_request_props>(config_data);
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        result = self.commit_response::<dpu_vpd>(vpd);
        match result {
            Ok(_) =>{
            }
            Err(e) => {
                return Err(e);
            }
        }
        match self.signal_used_queue() {
            Ok(_) => {
            }
            Err(e) => {
                return Err(e);
            }
        }
        result
    }
}