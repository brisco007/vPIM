#![allow(nonstandard_style)]
use memmap2::{MmapMut, MmapOptions};

use super::hardware::*;
use crate::virtio::{vpim::dpu_addr_translation::*, request_handlers::xfer_types::xfer_page_table};


pub const DPU_MAX_NR_CIS: usize = 8;
pub const DPU_MAX_NR_GROUPS: usize = 8;
pub const NR_OF_WRAM_BANKS: usize = 4;
pub const NB_MAX_REPAIR_ADDR: usize = 4;
pub const MAX_CHANNELS: u8 = 31;
pub type dpu_ci_bitfield_t = u8;
pub type dpu_member_id_t = u8;
pub type dpu_slice_id_t = u8;
pub type dpu_group_id_t = u8;
pub type dpu_type_t = u8;
pub type dpu_bitfield_t = u8;
pub type dpu_rank_id_t = u8;
pub type dpu_selected_mask_t = u32;
/* Size in IRAM */
pub type iram_size_t = u16;
/* Size in WRAM */
pub type wram_size_t = u32;

/* Size in MRAM */
pub type mram_size_t = u32;

#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_slice_target {
	aType: u32,
    dpu_id: dpu_member_id_t ,
    group_id: dpu_group_id_t 
}

pub enum DPURankStatus {
    DPU_RANK_SUCCESS,
    DPU_RANK_COMMUNICATION_ERROR,
    DPU_RANK_BACKEND_ERROR,
    DPU_RANK_SYSTEM_ERROR,
    DPU_RANK_INVALID_PROPERTY_ERROR,
    DPU_RANK_ENODEV,
}

#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_control_interface_context {
    pub fault_decode: dpu_ci_bitfield_t,
    pub fault_collide: dpu_ci_bitfield_t,
    pub color: dpu_ci_bitfield_t,
    /// Used for the current application to hold slice info
    pub slice_info: [dpu_configuration_slice_info_t; DPU_MAX_NR_CIS]
}

/* #[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_debug_context_t {
    dpu_ci_bitfield_t
        debug_color; // color expected when a debugger takes the control, so that we restore the same color when leaving.

    uint32_t debug_result[DPU_MAX_NR_CIS]; // Contains the result when the debugger attached the host application

    struct dpu_configuration_slice_info_t
        debug_slice_info[DPU_MAX_NR_CIS]; // Used by the debugger when attaching a process: is a copy of the above structure.

    bool is_rank_for_debugger;

    struct dpu_circular_buffer_commands_t cmds_buffer;
}; */

#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_memory_repair_address_t {
    pub address: u32,
    pub faulty_bits: u64,
}

#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_memory_repair_t {
    pub corrupted_address: [dpu_memory_repair_address_t; NB_MAX_REPAIR_ADDR],
    pub nr_of_corrupted_addresses: u32,
    pub fail_to_repair: bool,
}

#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct RamRepair {
    pub iram_repair: dpu_memory_repair_t,
    pub wram_repair: [dpu_memory_repair_t; NR_OF_WRAM_BANKS],
}

#[repr(C)]
#[derive(Clone,Copy, Debug, PartialEq, Eq)]
pub struct dpu_rank_request_props {
    pub is_owned : u8,
    pub rank_count : u8,
    pub channel_id : u8,
    pub backend_id : u8,
    pub dpu_chip_id : u8,
    pub rank_id : u8,
    pub capabilities : u64,
    pub mode: u8,
    pub debug_mode: u8,
    pub rank_index : u8,
    pub usage_count : u32,
    pub mcu_version : [u8;128],
    pub part_number : [u8;20],
    pub dimm_sn : [u8;10],
}

impl Default for dpu_rank_request_props {
    fn default() -> Self {
        dpu_rank_request_props { is_owned: 0,
            rank_count: 0,
            channel_id: 0, 
            backend_id: 0,
            dpu_chip_id: 0,
            rank_id: 0, 
            capabilities: 0, mode: 0,
            debug_mode: 0, 
            rank_index: 0,
            usage_count: 0, 
            mcu_version: [0;128], 
            part_number: [0;20],
            dimm_sn: [0;10] }
    }
}


#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_t {
    //reference to a dpu rank
    //pub rank: &dpu_rank_t,
    pub slice_id: dpu_slice_id_t,
    pub dpu_id: dpu_member_id_t,
    pub enabled: bool,
    pub repair: RamRepair,
/*     /* Used by high-level API and DPU logging feature */
    struct dpu_program_t *program; */
}

#[repr(C)]
#[derive(Debug)]
pub struct dpu_rank_t {
    pub type_: dpu_type_t,
    pub rank_id: dpu_rank_id_t,
    pub description: dpu_description_t,
    pub runtime: dpu_runtime_state_t,
    //pub debug: dpu_debug_context_t,
    pub dpu_addresses: u32,
    pub channel_id: u8,
    pub matrix : DpuTransferMatrix,
    pub callback_matrix : DpuTransferMatrix,
    pub dpus: Vec<dpu_t>,
    pub nr_dpus_enabled: u32,
    pub cmds: [u64; DPU_MAX_NR_CIS],
    pub data: [u64; DPU_MAX_NR_CIS],
    pub numa_node: i32,
    pub control_interfaces : [u64; DPU_MAX_NR_CIS],
    pub dpu_chip_id: u8,
    pub nb_cis: u8,
    pub region_size: usize,
    pub mmap: MmapMut,
    pub internal_data : xeon_sp_private,
    pub config_data : dpu_rank_request_props
}

impl dpu_rank_t {
    //create functions to update the dpu_rank_t struct fields
    pub fn set_type(&mut self, type_: dpu_type_t) {
        self.type_ = type_;
    }
    pub fn set_rank_id(&mut self, rank_id: dpu_rank_id_t) {
        self.rank_id = rank_id;
    }
    pub fn set_vm_rank_config_data(&mut self, cfg_data: dpu_rank_request_props) {
        self.config_data = cfg_data;
    }
    pub fn set_description(&mut self, description: dpu_description_t) {
        self.description = description;
        self.description.hw.signature.chip_id =  self.dpu_chip_id ;
        self.description.configuration.api_must_switch_mram_mux = true;
        self.description.configuration.init_mram_mux = true;
    }
    pub fn set_runtime(&mut self, runtime: dpu_runtime_state_t) {
        self.runtime = runtime;
    }
    pub fn set_ci_slice_info(&mut self, i:usize, data:u64){
        self.runtime.control_interface.slice_info[i].byte_order = data;
    }
    pub fn set_dpu_addresses(&mut self, dpu_addresses: u32) {
        self.dpu_addresses = dpu_addresses;
    }
    pub fn set_channel_id(&mut self, channel_id: u8) {
        self.channel_id = channel_id;
    }
    pub fn set_matrix(&mut self, matrix: DpuTransferMatrix) {
        self.matrix = matrix;
    }
    pub fn set_callback_matrix(&mut self, callback_matrix: DpuTransferMatrix) {
        self.callback_matrix = callback_matrix;
    }
    pub fn set_dpus(&mut self, dpus: Vec<dpu_t>) {
        self.dpus = dpus;
    }
    pub fn set_nr_dpus_enabled(&mut self, nr_dpus_enabled: u32) {
        self.nr_dpus_enabled = nr_dpus_enabled;
    }
    pub fn set_cmds(&mut self, cmds: [u64; DPU_MAX_NR_CIS]) {
        self.cmds = cmds;
    }
    pub fn set_data(&mut self, data: [u64; DPU_MAX_NR_CIS]) {
        self.data = data;
    }
    pub fn set_numa_node(&mut self, numa_node: i32) {
        self.numa_node = numa_node;
    }
    pub fn set_control_interfaces(&mut self, control_interfaces: [u64; DPU_MAX_NR_CIS]) {
        self.control_interfaces = control_interfaces;
    }
    pub fn set_dpu_chip_id(&mut self, dpu_chip_id: u8) {
        self.dpu_chip_id = dpu_chip_id;
    }
    pub fn set_nb_cis(&mut self, nb_cis: u8) {
        self.nb_cis = nb_cis;
    }
    pub fn set_region_size(&mut self, region_size: usize) {
        self.region_size = region_size;
    }
    pub fn do_mmap(&mut self, rank_fs :&dpu_rank_fs) {
        self.mmap = unsafe {
            MmapOptions::new().len(self.region_size)
           .map_mut(rank_fs.fdpu_dax.as_ref().unwrap()).unwrap()
       };
    }
    pub fn do_unmap(& self) {
       // drop(self.mmap)
    }
}



#[derive(Clone,Copy,Debug)]
pub struct U64Ptr(pub *mut u64);
unsafe impl Send for U64Ptr{}
unsafe impl Sync for U64Ptr{}
impl U64Ptr {
    pub fn toU8Ptr(&self) -> U8Ptr {
        U8Ptr(self.0 as *mut u8)
    }
    pub fn toU32Ptr(&self) -> U32Ptr {
        U32Ptr(self.0 as *mut u32)
    }
    pub fn toI32Ptr(&self) -> I32Ptr {
        I32Ptr(self.0 as *mut i32)
    }
    pub fn duplicate(&self)-> U64Ptr{
        U64Ptr(self.0)
    }
    pub fn fromConstPtr(ptr : *const u64) -> U64Ptr {
        U64Ptr(ptr as *mut u64)
    }
    pub fn fromArray( array : &mut [u8]) -> U64Ptr {
        U64Ptr(array.as_mut_ptr() as *mut u64)
    }
    pub fn fromArrayU64( array : &mut [u64]) -> U64Ptr {
        U64Ptr(array.as_mut_ptr())
    }
}

#[derive(Clone,Copy,Debug)]
pub struct U32Ptr(pub *mut u32);
unsafe impl Send for U32Ptr{}
unsafe impl Sync for U32Ptr{}
impl U32Ptr {
    pub fn toU8Ptr(&mut self) -> U8Ptr {
        U8Ptr(self.0 as *mut u8)
    }
    pub fn toU64Ptr(&mut self) -> U64Ptr {
        U64Ptr(self.0 as *mut u64)
    }
    pub fn fromArray( array : &mut [u8]) -> U32Ptr {
        U32Ptr(array.as_mut_ptr() as *mut u32)
    }
    pub fn fromArrayU64( array : &mut [u64]) -> U64Ptr {
        U64Ptr(array.as_mut_ptr())
    }
}
#[derive(Clone,Copy,Debug)]
pub struct I32Ptr(pub *mut i32);
unsafe impl Send for I32Ptr{}
unsafe impl Sync for I32Ptr{}
impl I32Ptr {
    pub fn toU8Ptr(&mut self) -> U8Ptr {
        U8Ptr(self.0 as *mut u8)
    }
    pub fn toU64Ptr(&mut self) -> U64Ptr {
        U64Ptr(self.0 as *mut u64)
    }
    pub fn fromU64Ptr(ptr : U64Ptr) -> I32Ptr {
        I32Ptr(ptr.0 as *mut i32)
    }
    pub fn fromArray( array : &mut [u8]) -> U32Ptr {
        U32Ptr(array.as_mut_ptr() as *mut u32)
    }
    pub fn fromArrayU64( array : &mut [u64]) -> U64Ptr {
        U64Ptr(array.as_mut_ptr())
    }
}
#[derive(Clone,Copy,Debug)]
pub struct U8Ptr(pub *mut u8);
unsafe impl Send for U8Ptr{}
unsafe impl Sync for U8Ptr{}

impl U8Ptr {
    pub fn toU64Ptr(&mut self) -> U64Ptr {
        U64Ptr(self.0 as *mut u64)
    }
    pub fn toU32Ptr(&mut self) -> U32Ptr {
        U32Ptr(self.0 as *mut u32)
    }
    pub fn fromArray( array : &mut [u8]) -> U8Ptr {
        U8Ptr(array.as_mut_ptr())
    }
    pub fn fromArrayU64( array : &mut [u64]) -> U64Ptr {
        U64Ptr(array.as_mut_ptr())
    }
    pub fn toU64(&mut self) -> u64 {
        self.0 as u64
    }
    pub fn fromU64( value : u64) -> U8Ptr {
        U8Ptr(value as *mut u8)
    }
}

#[derive(Clone,Copy,Debug)]
pub struct xferPageTablePtr(pub *mut xfer_page_table);
unsafe impl Send for xferPageTablePtr{}
unsafe impl Sync for xferPageTablePtr{}
impl xferPageTablePtr {
    pub fn fromArray( array : &[xfer_page_table]) -> xferPageTablePtr {
        xferPageTablePtr((array.as_ptr() as *const xfer_page_table) as *mut xfer_page_table)
    }
    pub fn fromArrayMut( array : &mut [xfer_page_table]) -> xferPageTablePtr {
        xferPageTablePtr(array.as_mut_ptr() as *mut xfer_page_table)
    }
}
