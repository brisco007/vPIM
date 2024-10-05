//use memmap::MmapOptions;
#![allow(nonstandard_style)]

use super::request_handlers::xfer_types::{dpu_transfer_mram_address, dpu_transfer_mram_batch};
/// Expected major number of the VPIMdevice
pub const DPU_MODULE_EXPECTED_MAJOR: i32 = 6;
/// Expected minor number of the VPIMdevice
pub const DPU_MODULE_MIN_MINOR: i32 = 4;
/// Maximum size of thread default
pub const DPU_XFER_THREAD_CONF_DEFAULT: i32 = std::i32::MAX;
/// Max number of DPUs 
pub const MAX_NR_DPUS_PER_RANK: usize = 64;

pub const MAX_THREAD_PER_POOL : u8 = 4;
/// Thread configuration for transfer
#[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
pub struct DpuTransferThreadConfiguration {
    pub nb_threads_per_pool: u8,
    pub threshold_1_threads: i32,
    pub threshold_2_threads: i32,
    pub threshold_4_threads: i32,
}
///Structure to use when considering interleaving
#[derive(Default,Debug, Clone, Copy, PartialEq, Eq)]
pub struct DpuRegionInterleaving {
    //per rank infos 
     pub nb_ci : u8,
     pub nb_dpus_per_ci : u8,
     pub mram_size : u32
}

///DPU ransfer matrix structure
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DpuTransferMatrix{
    pub(crate) ptr: [u8; MAX_NR_DPUS_PER_RANK],
    pub(crate) offset: usize,
    pub(crate) size: usize
}

#[repr(C)]
 #[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum thread_mram_xfer {
    ThreadMramXferRead,
    ThreadMramXferWrite
}

#[repr(C)]
 #[derive(Debug)]
///private data. should be generic but we only work on xeon for now
pub struct xeon_sp_private {
    pub direction: thread_mram_xfer,
    pub xfer_matrix: Option<Box<dpu_transfer_mram_address>>,
    pub xfer_matrices: Option<Box<dpu_transfer_mram_batch>>,
    pub nb_dpus_per_ci: u8,
    pub nb_threads_for_xfer: u8,
    pub nb_threads : u8,
    pub stop_thread: bool
}



/// Backend description of the CPU/BIOS configuration address translation:
///  interleave: Describe the machine configuration, retrieved from ACPI table
/// 		and dpu_chip_id_info: ACPI table gives info about physical
/// 		topology (number of channels, number of dimms...etc)
/// 		and the dpu_chip_id whose configuration is hardcoded
/// 		into dpu_chip_id_info.h (number of dpus, size of MRAM...etc).
///  init_rank: Init data structures/threads for a single rank
///  destroy_rank: Destroys data structures/threads for a single rank
///  write_to_cis: Writes blocks of 64 bytes that targets all CIs. The
/// 		 backend MUST:
/// 			- interleave
/// 			- byte order
/// 			- nopify and send MSB
/// 		 bit ordering must be done by upper software layer since only
/// 		 a few commands require it, which is unknown at this level.
///  read_from_cis: Reads blocks of 64 bytes from all CIs, same comment as
/// 		  write_block_to_ci.
///  write_to_rank: Writes to MRAMs using the matrix of descriptions of
/// 		  transfers for each dpu.
///  read_from_rank: Reads from MRAMs using the matrix of descriptions of
/// 		   transfers for each dpu.
#[repr(C)]
 #[derive(Debug,Copy, Default,Clone)]
 pub struct DpuRegionAddressTranslation {
    ///Physical topology
   pub interleave: DpuRegionInterleaving,
    /// Id exposed through sysfs for userspace.
   pub backend_id: u8,
    /// PERF, SAFE, HYBRID & MRAM, HYBRID & CTL IF, ... 
   pub capabilities: u64,
    /// Thread configuration to perform MRAM transfer
   pub xfer_thread_conf: DpuTransferThreadConfiguration,
    /// Read once
   pub one_read: bool,
}

//Usage modes
///Undefined mode means no mode works there
pub const DPU_REGION_MODE_UNDEFINED : i32 = 0;
/// Perf mode means the backend is optimized for performance, we can use MMAP
pub const DPU_REGION_MODE_PERF :i32 = 1;
/// Safe mode means the backend is optimized for safety, we can use ioctls
pub const DPU_REGION_MODE_SAFE :i32 = 2;
