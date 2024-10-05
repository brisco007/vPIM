#![allow(nonstandard_style)]
use std::{collections::VecDeque, ops::{DerefMut, Deref}};
use vm_memory::ByteValued;
use crate::virtio::{utilities::types::U64Ptr, dpu_addr_translation::MAX_NR_DPUS_PER_RANK};

pub const SZ_4K: u16 = 4096;
pub const SZ_1K: u16 = 1024;
pub const SZ_1M: u32 = 1048576;

pub const XFER_BATCH_SIZE : usize= 32;
#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct TestStruct2 {
    pub my_inc : u32,
    pub name : [i32;5]
}

//WARNING : JUST FOR TEST PURPOSES
#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct TestStruct {
    pub my_inc : u32,
    pub name : [i32;5],
    pub my_struct : Option<TestStruct2>
}

#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct xfer_info{
	pub nb_pages: u64,
	pub off_first_page: u32
}

#[repr(C)]
#[derive(Clone,Copy, Debug, PartialEq)]
pub(crate) struct xfer_info_batch{
	pub content: [xfer_info;XFER_BATCH_SIZE]
}

impl Default for xfer_info_batch {
    fn default() -> Self {
        xfer_info_batch {
            content: [xfer_info::default(); XFER_BATCH_SIZE],
        }
    }
}

#[repr(C)]
#[derive(Clone, Debug, PartialEq)]
pub struct xfer_pages {
    pub pages: VecDeque<page_data>,
	pub nb_pages: u64,
    pub off_first_page: u32
}

impl Default for xfer_pages {
    fn default() -> Self {
        Self {
            pages: VecDeque::new(),
            nb_pages: 0,
            off_first_page: 0,
        }
    }
}
///The default page_data contains a zeroed payload array of size 4K
#[repr(C)]
#[derive(Clone,Copy, Debug, PartialEq)]
pub struct page_data{
    pub payload: [u8; 4096]
}

impl Default for page_data {
    fn default() -> Self {
        Self { payload: [0; 4096]}
    }
}

#[repr(C)]
#[derive(Clone, Default,Debug, PartialEq)]
pub struct dpu_transfer_mram {
    pub ptr : VecDeque<xfer_pages>,
	pub offset_in_mram : u32,
	pub size : u32
}

impl Deref for dpu_transfer_mram {
    type Target = dpu_transfer_mram;

    fn deref(&self) -> &Self::Target {
        &self
    }
}

impl DerefMut for dpu_transfer_mram {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self
    }
}

#[repr(C)]
#[derive(Clone,Copy, Debug, Default, PartialEq)]
pub(crate) struct dpu_transfer_info {
	pub offset_in_mram : u32,
	pub size : u32
}
#[repr(C)]
#[derive(Clone,Copy, Debug, PartialEq)]
pub(crate) struct dpu_transfer_info_batch {
	pub content: [dpu_transfer_info;XFER_BATCH_SIZE]
}

impl Default for dpu_transfer_info_batch {
    fn default() -> Self {
        dpu_transfer_info_batch {
            content: [dpu_transfer_info::default(); XFER_BATCH_SIZE],
        }
    }
}

/*#[repr(C)]
#[derive(Debug)]
pub struct xfer_page_table{
    pub nb_pages: u64,
    pub off_first_page: u32,
    pub pages: VecDeque<u64>,
}*/

//NEW
#[repr(C)]
#[derive(Debug,Copy,Clone)]
pub struct xfer_page_table{
    pub nb_pages: u64,
    pub off_first_page: u32,
    pub pages: U64Ptr,
}
#[repr(C)]
#[derive(Debug,Copy,Clone)]
pub struct xfer_page_table_batch{
    pub nb_pages: u64,
    pub off_first_page: u32,
    pub start: U64Ptr,
    pub end: U64Ptr
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct dpu_transfer_mram_address{
    pub ptr : [xfer_page_table; MAX_NR_DPUS_PER_RANK], 
	pub offset_in_mram : u32,
	pub size : u32
}
#[repr(C)]
#[derive(Debug)]
pub struct dpu_transfer_mram_single_batch{
    pub ptr : [xfer_page_table_batch; MAX_NR_DPUS_PER_RANK], 
	pub offset_in_mram : u32,
	pub size : u32
}
use std::ptr::null_mut;

impl Default for dpu_transfer_mram_address {
    fn default() -> Self {
        dpu_transfer_mram_address {
            ptr: [xfer_page_table {
                nb_pages: 0,
                off_first_page: 0,
                pages: U64Ptr(null_mut())
            }; MAX_NR_DPUS_PER_RANK],
            offset_in_mram: 0,
            size: 0
        }
    }
}

//batched matrix
#[repr(C)]
#[derive(Debug)]
pub struct dpu_transfer_mram_batch{
    pub matrix: [dpu_transfer_mram_address; XFER_BATCH_SIZE],
    pub nb_matrices: u32
}

impl Default for dpu_transfer_mram_batch {
    fn default() -> Self {
        dpu_transfer_mram_batch {
            matrix: [dpu_transfer_mram_address::default(); XFER_BATCH_SIZE],
            nb_matrices: 0, // default value for nb_matrices
        }
    }
}

unsafe impl ByteValued for TestStruct {}
unsafe impl ByteValued for TestStruct2 {}
unsafe impl ByteValued for xfer_info {}
unsafe impl ByteValued for xfer_info_batch {}
unsafe impl ByteValued for dpu_transfer_info {}
unsafe impl ByteValued for dpu_transfer_info_batch {}
unsafe impl ByteValued for page_data {}
