#![allow(nonstandard_style)]
use vm_memory::ByteValued;
use crate::virtio::vpim::utilities::types::*;
use std::fs::File;


#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_configuration_slice_info_t {
	pub byte_order: u64,
	pub structure_value: u64,
    pub slice_target: dpu_slice_target,
	pub host_mux_mram_state: dpu_bitfield_t,
    pub dpus_per_group: [dpu_selected_mask_t; DPU_MAX_NR_GROUPS],
	pub enabled_dpus: dpu_selected_mask_t,
	pub all_dpus_are_enabled: bool,
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_control_interface_context {
	pub fault_decode: dpu_ci_bitfield_t,
	pub fault_collide: dpu_ci_bitfield_t, 
    pub color: dpu_ci_bitfield_t,
	pub slice_info: [dpu_configuration_slice_info_t; DPU_MAX_NR_CIS]
}
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct dpu_control_interface_array{
    pub(crate)CIS: [u64; DPU_MAX_NR_CIS]
}
unsafe impl ByteValued for dpu_control_interface_array {}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq, Eq)]
pub struct dpu_run_context_t {
    pub(crate) dpu_running: [dpu_bitfield_t; DPU_MAX_NR_CIS],
    pub(crate) dpu_in_fault: [dpu_bitfield_t; DPU_MAX_NR_CIS],
	pub(crate) nb_dpu_running: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_runtime_state_t {
	pub control_interface: dpu_control_interface_context,
	pub run_context: dpu_run_context_t
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_bit_config {
	pub cpu2dpu: u16,
	pub dpu2cpu: u16,
	pub nibble_swap: u8,
	pub stutter: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct Signature{
    pub chip_id: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_carousel_config {
	pub cmd_duration: u8,
	pub cmd_sampling: u8,
	pub res_duration: u8,
	pub res_sampling: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct Timings{
    /** Carousel configuration. */
    pub carousel: dpu_carousel_config,
    /** Clock frequency (in MHz). */
    pub fck_frequency_in_mhz: u32,
    /** Reset duration (in cycles). */
    pub reset_wait_duration: u8,
    /** Temperature threshold. */
    pub std_temperature: u8,
    /** Clock frequency division. */
    pub clock_division: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct Topology{
    /** The number of Control Interfaces in a DPU rank. */
   pub  nr_of_control_interfaces: u8,
    /** The number of DPUs per Control Interface. */
   pub nr_of_dpus_per_control_interface: u8
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct Memories{
    /** MRAM size in bytes. */
    pub mram_size: mram_size_t,
    /** WRAM size in words. */
    pub wram_size: wram_size_t,
    /** IRAM size in instructions. */
    pub iram_size: iram_size_t
}
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct Dpu{
    /** Bit shuffling configuration. */
    pub pcb_transformation: dpu_bit_config,
    /** Number of hardware threads per DPU. */
    pub nr_of_threads: u8,
    /** Number of atomic bit per DPU. */
    pub nr_of_atomic_bits: u32,
    /** Number of notify bit per DPU. */
    pub nr_of_notify_bits: u32,
    /** Number of work registers per DPU thread. */
    pub nr_of_work_registers_per_thread: u8
} 
#[repr(C)]
#[derive(Clone,Copy, Default, Debug, PartialEq, Eq)]
pub struct dpu_hw_description_t {
    /** Signature of the chip. */
    pub signature: Signature,
    /** Timing configuration. */
    pub timings: Timings,
    /** DPU rank topology. */
    pub topology: Topology,
    /** Memory information. */
    pub memories: Memories,
    /** DPU information. */
    pub dpu: Dpu
}
#[repr(C)]
#[derive(Clone, Default,Copy, Debug, PartialEq, Eq)]
pub struct Configuration {
    /** Whether the MRAM accesses are to be made via DPU program and the WRAM. */
    pub mram_access_by_dpu_only:bool,
    /** Whether the MRAM accesses need to be preceeded by a MUX switch. */
    pub api_must_switch_mram_mux: bool,
    /** Whether the MRAM MUX must be initialized. */
    pub init_mram_mux: bool,

    /** Whether the IRAM repair procedure must be executed. */
    pub do_iram_repair: bool,
    /** Whether the WRAM repair procedure must be executed. */
    pub do_wram_repair: bool,
    /** Whether the VPD information should be ignored when repairing. */
    pub ignore_vpd: bool,

    /** Whether the cycle accurate behavior must be enabled (FPGA only). */
    pub enable_cycle_accurate_behavior: bool,

    /** Whether some defensive checks should be disabled. */
    pub disable_api_safe_checks: bool,

    /** Whether the reset procedure should be disabled when allocating a new rank. */
    pub disable_reset_on_alloc: bool
} 

#[repr(C)]
#[derive(Clone, Default,Copy, Debug, PartialEq, Eq)]
pub struct dpu_description_t {
    pub hw: dpu_hw_description_t,
    pub dpu_type: dpu_type_t,
    pub configuration: Configuration,
    pub refcount: u32,
    pub internals : u64
}

#[repr(C)]
#[derive(Default,Debug)]
pub struct dpu_rank_fs {
    pub rank_path : String,
    pub dax_path : String,
    pub rank_id : u8,
    pub fdpu_rank: Option<File>,
    pub fdpu_dax: Option<File>,
}

unsafe impl ByteValued for dpu_runtime_state_t {}
unsafe impl ByteValued for dpu_bit_config {}
unsafe impl ByteValued for dpu_hw_description_t {}
unsafe impl ByteValued for dpu_rank_request_props {}
