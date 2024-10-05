#![allow(nonstandard_style)]
#![allow(dead_code)]
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

#[cfg(test)]
pub(crate) mod tests {
    use std::{collections::VecDeque, thread};

    use crate::virtio::{VPIMDevice, utilities::types::U64Ptr};
    
    use super::*;

    #[test]
    fn test_handler_write_to_cis() {
        let mut vpim = VPIMDevice::new(1
        ).unwrap();
        vpim.config();
        //write to cis
       
        //soft reset

/*         let ci_array2: [u64;NB_REAL_CIS] = [143833713099210496; NB_REAL_CIS];
        let mut results_ci2: [u64;NB_REAL_CIS] = [0u64;NB_REAL_CIS];
        let out = vpim.addr_translation.write_to_cis(Arc::clone(&vpim.rank), ci_array2.as_ptr());
        match out {
            Ok(()) => (),
            Err(e) => panic!("{}",e)
        } 
        let out2 = vpim.addr_translation.read_from_cis(Arc::clone(&vpim.rank), results_ci2.as_mut_ptr());
        println!("results_ci for soft reset  : {:?}", results_ci2);
*/

        //bit order
        let ci_array: [u64;NB_REAL_CIS] = [72057594037927936; NB_REAL_CIS];
        let mut results_ci: [u64;NB_REAL_CIS] = [0u64;NB_REAL_CIS];
        let out = vpim.addr_translation.write_to_cis(Arc::clone(&vpim.rank), ci_array.as_ptr());
        match out {
            Ok(()) => (),
            Err(e) => panic!("{}",e)
        }
        let mut nr_retries = 5; 
        let mut good_result = false;
        while nr_retries > 0 && !good_result {
            let out2 = vpim.addr_translation.read_from_cis(Arc::clone(&vpim.rank), results_ci.as_mut_ptr());
            match out2 {
                Ok(()) => {
                    println!("nr_retries left : {}", nr_retries);
                    println!("results_ci  : {:?}", results_ci);
                    if results_ci != [1095477249058;NB_REAL_CIS] { //0xff0f884422
                        results_ci =  [0u64;NB_REAL_CIS];
                        nr_retries-=1;
                    } else {
                        good_result = true;
                    }
                },
                Err(e) => panic!("{}",e)
            }
        }
        assert!(nr_retries > 0 && good_result);
    }

    #[test]
    fn test_dpu_alloc() {
        let mut vpim = VPIMDevice::new(1
        ).unwrap();
        vpim.config();

        //soft reset
        let ci_array: [u64;NB_REAL_CIS] = [143833713099210496; NB_REAL_CIS];
        let mut results_ci: [u64;NB_REAL_CIS] = [0u64;NB_REAL_CIS];
        let out = vpim.addr_translation.write_to_cis(Arc::clone(&vpim.rank), ci_array.as_ptr());
        match out {
            Ok(()) => (),
            Err(e) => panic!("{}",e)
        }
        let mut nr_retries = 100; 
        let mut good_result = false;
        while nr_retries > 0 && !good_result {
            let out2 = vpim.addr_translation.read_from_cis(Arc::clone(&vpim.rank), results_ci.as_mut_ptr());
            match out2 {
                Ok(()) => {
                    println!("nr_retries left : {}", nr_retries);
                    println!("results_ci  : {:?}", results_ci);
                    if results_ci != [71777214277877760;NB_REAL_CIS] {
                        nr_retries-=1;
                    } else {
                        good_result = true;
                    }
                },
                Err(e) => panic!("{}",e)
            }
        }
        assert!(nr_retries > 0 && good_result);
        
        //bit config
        let ci_array: [u64;NB_REAL_CIS] = [72057594037927936; NB_REAL_CIS];
        let mut results_ci: [u64;NB_REAL_CIS] = [0u64;NB_REAL_CIS];
        let out = vpim.addr_translation.write_to_cis(Arc::clone(&vpim.rank), ci_array.as_ptr());
        match out {
            Ok(()) => (),
            Err(e) => panic!("{}",e)
        }
        let mut nr_retries = 100; 
        let mut good_result = false;
        while nr_retries > 0 && !good_result {
            let out2 = vpim.addr_translation.read_from_cis(Arc::clone(&vpim.rank), results_ci.as_mut_ptr());
            match out2 {
                Ok(()) => {
                    println!("nr_retries left : {}", nr_retries);
                    println!("results_ci  : {:?}", results_ci);
                    if results_ci != [1095477249058;NB_REAL_CIS] {
                        nr_retries-=1;
                    } else {
                        good_result = true;
                    }
                },
                Err(e) => panic!("{}",e)
            }
        }
        assert!(nr_retries > 0 && good_result);

          //shuffling box config
          let ci_array: [u64;NB_REAL_CIS] = [72309265781751808; NB_REAL_CIS];
          let mut results_ci: [u64;NB_REAL_CIS] = [0u64;NB_REAL_CIS];
          let out = vpim.addr_translation.write_to_cis(Arc::clone(&vpim.rank), ci_array.as_ptr());
          match out {
              Ok(()) => (),
              Err(e) => panic!("{}",e)
          }
          let mut nr_retries = 100; 
          let mut good_result = false;
          while nr_retries > 0 && !good_result {
              let out2 = vpim.addr_translation.read_from_cis(Arc::clone(&vpim.rank), results_ci.as_mut_ptr());
              match out2 {
                  Ok(()) => {
                      println!("nr_retries left : {}", nr_retries);
                      println!("results_ci  : {:?}", results_ci);
                      if results_ci != [71777214538466338;NB_REAL_CIS] {
                          nr_retries-=1;
                      } else {
                          good_result = true;
                      }
                  },
                  Err(e) => panic!("{}",e)
              }
          }
          assert!(nr_retries > 0 && good_result);
    }

    #[test]
    fn test_handler_write_to_rank() {/* 
         let mut vpim = VPIMDevice::new(
            0,
            false,
            0,
           0,
        ).unwrap();
        vpim.config();
        let mut my_ptr : VecDeque<xfer_pages> = VecDeque::new();

        for _ in 0..64 {
            my_ptr.push_back(
                xfer_pages { pages: VecDeque::new(), nb_pages: 0, off_first_page: 0 }
            )
        }
        my_ptr[0].nb_pages = 17;
        my_ptr[0].off_first_page = 1200;
        let mut pg_data : VecDeque<page_data> = VecDeque::new();
        for _ in 0..17 {
            pg_data.push_back(
                page_data {
                    payload: [255;4096],
                }
            );
        }
        my_ptr[0].pages = pg_data;

        let xfer_matrix : dpu_transfer_mram;
        xfer_matrix = dpu_transfer_mram {
            ptr: my_ptr,
            offset_in_mram: 0,
            size: 65536,
        };
        //test zeroes
         let out = vpim.addr_translation.write_to_rank(Arc::clone(&vpim.rank), Box::<dpu_transfer_mram>::new(xfer_matrix));
        match out {
            Ok(()) => {
                println!("WE HAVE WRITTEN INTO THE RANK");
                ()
            },
            Err(e) => panic!("{}",e)
        }
        assert!(true); */
      /*   out = vpim.addr_translation.read_from_rank(Arc::clone(&vpim.rank));
        match out {
            Ok(()) => println!("the matrix \n{:#}", vpim.rank.lock().unwrap().internal_data.xfer_matrix.as_ref().unwrap().ptr.back().unwrap().pages.back().unwrap().payload[0])
            ,
            Err(e) => panic!("{}",e)
        }  */

    }
    #[test]
    fn test_handler_read_from_rank() {
    /*     let mut vpim = VPIMDevice::new(
            0,
            false,
            0,
           0,
        ).unwrap();
        vpim.config();
        //write to cis
        //test zeroes
         let out = vpim.addr_translation.read_from_rank(Arc::clone(&vpim.rank));
        match out {
            Ok(()) => println!("the matrix \n{:#}", vpim.rank.lock().unwrap().internal_data.xfer_matrix.as_ref().unwrap().ptr.back().unwrap().pages.back().unwrap().payload[0])
            ,
            Err(e) => panic!("{}",e)
        } */
    }

    #[test]
    fn test_threads() {
        let mut myArray : [u64;8] = [0;8];
        let arrayPtr = myArray.as_mut_ptr();
        let arrayPtrSafe = U64Ptr(arrayPtr);

        crossbeam::thread::scope(|s|{
            s.spawn(
                |_| {
                   println!("In thread 1: starting working on array");
                   for i in 0..7 {
                       unsafe {
                        println!("[T1] Printing array elem : {}", *(arrayPtrSafe.0.add(i)));
                       }
                       thread::sleep_ms(2000);
                   }
               }
           );
           println!("badamou");
           s.spawn(
                |_| {
                   println!("In thread 2: starting working on array");
                   for i in 0..7 {
                       println!("[T2] writing data in array {}", i);
                       unsafe {
                        *(arrayPtrSafe.0.add(i)) =  (i as u64)*25;
                       }
                       thread::sleep_ms(2000);
                   }
               }
           );
           s.spawn(
                |_| {
                   println!("In thread 3: starting working on array");
                   for i in 0..8 {
                    unsafe {
                     println!("[T3] Printing array elem again : {}", *(arrayPtrSafe.0.add(i)));
                    }
                    thread::sleep_ms(2000);
                }
               }
           );
        }).unwrap();

        println!("finished working on my array ");
    }
}
