#![allow(nonstandard_style)]
#![allow(dead_code)]
#![allow(unused_imports)]
#![allow(unused_assignments)]
#![allow(unused_variables)]
#![allow(improper_ctypes)]
use std::{mem::size_of, sync::{Arc, Mutex}, arch::x86_64::{_mm_clflush, _mm_mfence, __m256i, _mm256_stream_si256, _mm256_load_si256, __m128i, _mm_set1_epi16, _mm_setr_epi8, _mm_load_ps, __m128, _mm_shuffle_ps, _mm_blend_ps, _mm_castsi128_ps, _mm_shuffle_epi8, _mm_castps_si128, _mm_storeu_ps, _mm_stream_ps}, cmp, ptr::null_mut, thread::{self, current}, ops::Add, time::Instant};
use std::time::Duration;
use core::ptr::write_volatile;
use vm_memory::{GuestMemoryMmap, GuestMemory, GuestAddress};
use crate::virtio::{
    vpim::dpu_addr_translation::*, 
    utilities::{types::{MAX_CHANNELS, dpu_rank_t, U64Ptr, U8Ptr,xferPageTablePtr}, 
    memory_utils::{memory_write_u8, memory_read_u8}}, 
    request_handlers::xfer_types::{dpu_transfer_mram, xfer_pages, page_data, dpu_transfer_mram_address, xfer_page_table, dpu_transfer_mram_batch, XFER_BATCH_SIZE, SZ_4K}
};
use std::arch::x86_64::{_mm256_set_epi8, _mm256_setr_epi32, _mm256_i32gather_epi32, _mm256_shuffle_epi8, _mm256_permutevar8x32_epi32, _mm256_storeu_si256};

pub const NB_REAL_CIS : usize = 8;
pub const NB_REAL_CIS_U8 : u8 = 8;
pub const NB_ELEM_MATRIX: usize = 8;
pub const BANK_CHUNK_SIZE: u64 = 0x20000;
pub const BANK_NEXT_CHUNK_OFFSET : u64 = 0x100000;
pub(crate) const NB_THREADS: u8 = 8;


extern "C" {
    pub(crate) fn byte_interleave_avx512(input: U64Ptr, output: U64Ptr, use_stream: i32);
    pub(crate) fn c_write_to_dpus(dst: U8Ptr, matrix: xferPageTablePtr, size_transfer : u32, off_mram: u32, idx : u8) -> u8;
    pub(crate) fn c_read_from_dpus(dst: U8Ptr, matrix: xferPageTablePtr, size_transfer : u32, off_mram: u32, idx : u8) -> u8;
}


/// function to apply byte interleaving
/// Arguments input: a reference to the input address to interleave
/// Arguments output: a reference to the output address to interleave
/// Return value: None
pub fn byte_interleave(input: *const u64, output: *mut u64) {
    unsafe{
        for i in 0..NB_ELEM_MATRIX {
            for j in 0..size_of::<u64>() {
                *(((output.add(i)) as *mut u8).add(j)) =  *((input.add(j) as *const u8).add(i))
            }
        }
    }
}

  
#[target_feature(enable = "avx2")]
pub unsafe fn byte_interleave_avx2(input: U64Ptr, output: U64Ptr, use_stream:bool) {
    let tm = _mm256_set_epi8(
        15,
        11,
        7,
        3,
        14,
        10,
        6,
        2,
        13,
        9,
        5,
        1,
        12,
        8,
        4,
        0,
        15,
        11,
        7,
        3,
        14,
        10,
        6,
        2,
        13,
        9,
        5,
        1,
        12,
        8,
        4,
        0
    );
    let vindex: __m256i = _mm256_setr_epi32(0, 8, 16, 24, 32, 40, 48, 56);
    let perm: __m256i = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
    
    //let load0: __m256i = _mm256_i32gather_epi32::<1>(input as *const i32, vindex);  
    let load0: __m256i = _mm256_i32gather_epi32(input.toI32Ptr().0, vindex,1);  
    //let load1: __m256i = _mm256_i32gather_epi32::<1>((input as *const u8).add(4) as *const i32, vindex);  
    let load1: __m256i = _mm256_i32gather_epi32((input.toU8Ptr().0).add(4) as *const i32, vindex,1);  

    let transpose0: __m256i = _mm256_shuffle_epi8(load0, tm);
    let transpose1: __m256i = _mm256_shuffle_epi8(load1, tm);

    let final0: __m256i = _mm256_permutevar8x32_epi32(transpose0, perm);
    let final1: __m256i = _mm256_permutevar8x32_epi32(transpose1, perm);

    //TODO use this for writing to ci only (with the boolean attribute set _mm256_stream_si256(mem_addr, a) it is for a non temporal usage)
    if use_stream {
        _mm256_stream_si256((output.toU8Ptr().0) as *mut __m256i, final0);
        _mm256_stream_si256(((output.toU8Ptr().0).add(32)) as *mut __m256i, final1);
    } else {
        _mm256_storeu_si256((output.toU8Ptr().0) as *mut __m256i, final0);
        _mm256_storeu_si256(((output.toU8Ptr().0).add(32)) as *mut __m256i, final1);
    }
   
}

 
#[target_feature(enable = "sse4.1")]
pub unsafe fn byte_interleave_sse4_1(input: *const u64, output: *mut u64, use_stream:bool) {
    let A: *const u8 = input as *const u8;
    let B: *mut u8 = output as *mut u8;

    let pshufbcnst_0: __m128i = _mm_setr_epi8(15, 7, 11, 3, 13, 5, 9, 1, 14, 6, 10, 2, 12, 4, 8, 0);
    let pshufbcnst_1: __m128i = _mm_setr_epi8(13, 5, 9, 1, 15, 7, 11, 3, 12, 4, 8, 0, 14, 6, 10, 2);
    let pshufbcnst_2: __m128i = _mm_setr_epi8(11, 3, 15, 7, 9, 1, 13, 5, 10, 2, 14, 6, 8, 0, 12, 4);
    let pshufbcnst_3: __m128i = _mm_setr_epi8(9, 1, 13, 5, 11, 3, 15, 7, 8, 0, 12, 4, 10, 2, 14, 6);

    let mut B0: __m128;
    let mut B1: __m128;
    let mut B2: __m128;
    let mut B3: __m128;
    let mut T0: __m128;
    let mut T1: __m128;
    let mut T2: __m128;
    let mut T3: __m128;

    B0 = _mm_load_ps((A as *const f32).add(0));
    B1 = _mm_load_ps((A as *const f32).add(16));
    B2 = _mm_load_ps((A as *const f32).add(32));
    B3 = _mm_load_ps((A as *const f32).add(48));

    B1 = _mm_shuffle_ps(B1, B1, 0b10110001);
    B3 = _mm_shuffle_ps(B3, B3, 0b10110001);
    T0 = _mm_blend_ps(B0, B1, 0b1010);
    T1 = _mm_blend_ps(B2, B3, 0b1010);
    T2 = _mm_blend_ps(B0, B1, 0b0101);
    T3 = _mm_blend_ps(B2, B3, 0b0101);

    B0 = _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(T0), pshufbcnst_0));
    B1 = _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(T1), pshufbcnst_1));
    B2 = _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(T2), pshufbcnst_2));
    B3 = _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(T3), pshufbcnst_3));

    T0 = _mm_blend_ps(B0, B1, 0b1010);
    T1 = _mm_blend_ps(B0, B1, 0b0101);
    T2 = _mm_blend_ps(B2, B3, 0b1010);
    T3 = _mm_blend_ps(B2, B3, 0b0101);
    T1 = _mm_shuffle_ps(T1, T1, 0b10110001);
    T3 = _mm_shuffle_ps(T3, T3, 0b10110001);

    if use_stream {
        _mm_stream_ps((B as *mut f32).add(0), T0);
        _mm_stream_ps((B as *mut f32).add(16), T1);
        _mm_stream_ps((B as *mut f32).add(32), T2);
        _mm_stream_ps((B as *mut f32).add(48), T3);
    } else {
        _mm_storeu_ps((B as *mut f32).add(0), T0);
        _mm_storeu_ps((B as *mut f32).add(16), T1);
        _mm_storeu_ps((B as *mut f32).add(32), T2);
        _mm_storeu_ps((B as *mut f32).add(48), T3);
    }
}

type IntlvMatrix = [[u64; size_of::<u64>()]; NB_ELEM_MATRIX];
//implement DpuRegionAddressTranslation 
impl DpuRegionAddressTranslation {
    //implement byte interleaving that takes as argument the matrix of input and output addresses
    pub fn byte_interleave(input: IntlvMatrix ) -> IntlvMatrix{
        let _j:usize;
        let _i:usize;
        let size = size_of::<u64>();
        let mut output: IntlvMatrix = [[0; size_of::<u64>()]; NB_ELEM_MATRIX];

        for _i in 0..NB_ELEM_MATRIX {
            for _j in 0..size {
                 output[_i][_j] = input[_j][_i];
            }
        }
       /*  unsafe {
            _mm512_storeu_si512(mem_addr, a)
        } */
        return output;
    }

}

pub fn channel_id_to_pool_id(channel_id: u8) -> u8
{
    let pool_id: u8;

    if channel_id == 0xFF {
        pool_id = MAX_CHANNELS - 1;
    }
    else{
         pool_id = channel_id;
    }
    return pool_id;
}

pub const NB_THREADS_1 : u8= 1;
pub const NB_THREADS_2 : u8= 2;
pub const NB_THREADS_4 : u8= 4;
pub const NB_THREADS_8 : u8= 8;

const THRESHOLD_1_THREAD: usize = 1024;
const THRESHOLD_2_THREADS: usize = 2048;
const THRESHOLD_4_THREADS: usize = 32 * 1024;
const NR_THREADS_PER_POOL: usize = 8;
pub const TRUE : i32 = 1;
pub const FALSE : i32 = 0;
pub fn nb_threads_for_xfer(size : u32) -> u8 {
    if size < THRESHOLD_1_THREAD as u32 {
        return NB_THREADS_1;
    } else if size < THRESHOLD_2_THREADS as u32 && NR_THREADS_PER_POOL >=2 {
        return NB_THREADS_2;
    } else if size < THRESHOLD_4_THREADS as u32 && NR_THREADS_PER_POOL >= 4 {
        return NB_THREADS_4;
    } else {
        return NB_THREADS_8;
    }
}

fn apply_address_translation_on_mram_offset(byte_offset: u32) -> usize
{
    /* We have observed that, within the 26 address bits of the MRAM address, we need to apply an address translation:
     *
     * virtual[13: 0] = physical[13: 0]
     * virtual[20:14] = physical[21:15]
     * virtual[   21] = physical[   14]
     * virtual[25:22] = physical[25:22]
     *
     * This function computes the "virtual" mram address based on the given "physical" mram address.
     */
    let mask_21_to_15: u32 = ((1 << (21 - 15 + 1)) - 1) << 15;
    let mask_21_to_14: u32 = ((1 << (21 - 14 + 1)) - 1) << 14;
    let bits_21_to_15: u32 = (byte_offset & mask_21_to_15) >> 15;
    let bit_14: u32 = (byte_offset >> 14) & 1;
    let unchanged_bits: u32 = byte_offset & !mask_21_to_14;
    let result = unchanged_bits | (bits_21_to_15 << 14) | (bit_14 << 21);
    return result as usize;
}



fn check_boundaries(current : *mut u64, end : *mut u64) {
    if current >= end {
        println!("ERROR : BAD ADDRESS! BUFFER OVERFLOW");
        panic!("ERROR : BAD ADDRESS! BUFFER OVERFLOW");
    }
}
