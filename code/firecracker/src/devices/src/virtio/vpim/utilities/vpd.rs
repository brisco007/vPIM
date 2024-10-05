#![allow(nonstandard_style)]

use std::mem::size_of;

use vm_memory::ByteValued;
///Maximum number of ranks per DIMMs
pub const VPD_MAX_RANK : usize =  4;
///The magic used in the VPD structure for UPMEM DIMMs.
pub const VPD_STRUCT_ID : &str = "UPMV";
///The current version in the VPD structure for UPMEM DIMMs.
pub const VPD_STRUCT_VERSION : i32 = 0x0004000;
///Maximum size for the VPD structure.
pub const VPD_MAX_SIZE : i32 = 2048;
///Special value used for undefined repair entry.
pub const VPD_UNDEFINED_REPAIR_COUNT: u16 = u16::MAX;

///Holds information about DPUs that contain repairs or that are disabled
#[repr(C)]
#[derive(Clone, Default,Copy, Debug, PartialEq, Eq)]
pub struct dpu_vpd_rank_data{
    ///Bitmap of disabled DPUs. 
    pub dpu_disabled : u64,
    ///Bitmap of WRAM requiring repairs (1 bit per DPU).
    pub wram_repair : u64,
    ///Bitmap of IRAM requiring repairs (1 bit per DPU).
    pub iram_repair : u64
}

///Description of a repair entry
#[repr(C)]
#[derive(Clone, Default, Copy, Debug, PartialEq, Eq)]
pub struct dpu_vpd_repair_entry{
    ///VPD_REPAIR_IRAM (`0`) or VPD_REPAIR_WRAM (`1`).
    pub iram_wram : u8,
    //TODO set this to the vpim rank id later on. for now, it is set to 0.
    ///Rank ID.
    pub rank : u8,
    /// Control Interface index.
    pub ci : u8,
    ///DPU member ID.
    pub dpu : u8,
    ///Bank number.
    pub bank : u8,
    ///Padding byte to align the following fields. 
    pub __padding : u8,
    ///Address to repair.
    pub address : u16,
    ///Bits to repair. 
    pub bits : u64,
}


#[repr(C)]
#[derive(Clone, Default,Copy, Debug, PartialEq, Eq)]
pub struct dpu_vpd_header{
    ///Contains 'U','P','M', 'V' for UPMEM VPD. (their ASCII code)
    pub struct_id : [u8; 4],
    ///The VPD structure version..
    pub struct_ver : u32,
    ///The VPD structure size.
    pub struct_size : u16,
    ///Total number of ranks on the DIMM. 
    pub rank_count : u8,
    ///Padding byte to align the following fields. 
    pub __padding__0: u8,
    ///Number of entries in the SRAM repairs list: -1 means no repair done yet. 
    pub repair_count : u16,
    ///Padding byte to align the following fields.
    pub __padding_1 : u16,
    ///Repair information for each DPU rank of the DIMM.
    pub ranks : [dpu_vpd_rank_data; 4]
}
/// NB_MAX_REPAIR represents this - `(VPD_MAX_SIZE - size_of<dpu_vpd_header>()) / size_of<dpu_vpd_repair_entry>()`
pub const NB_MAX_REPAIR : i32 = (VPD_MAX_SIZE - (size_of::<dpu_vpd_header>() as i32)) / (size_of::<dpu_vpd_repair_entry>() as i32);
//pub const NB_MAX_REPAIR : i32 = 121;

/// structure for VPD handling
#[repr(C)]
#[derive(Debug)]
pub struct dpu_vpd{
    /// VPD header
    pub vpd_header : dpu_vpd_header,
    /// Repair information enrties
    pub repair_entries : [dpu_vpd_repair_entry; NB_MAX_REPAIR as usize]
}

impl Default for dpu_vpd{
    fn default() -> Self {
        Self { 
            vpd_header: Default::default(), 
            repair_entries: [dpu_vpd_repair_entry::default(); NB_MAX_REPAIR as usize] 
        }
    }
}

impl Clone for dpu_vpd{
    fn clone(&self) -> Self {
        Self { vpd_header: self.vpd_header.clone(), repair_entries: self.repair_entries.clone() }
    }
}

impl Copy for dpu_vpd{}

unsafe impl ByteValued for dpu_vpd_header {}
unsafe impl ByteValued for dpu_vpd_repair_entry{}
unsafe impl ByteValued for dpu_vpd{}