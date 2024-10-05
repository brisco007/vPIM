#![allow(nonstandard_style)]
#![allow(dead_code)]
#![allow(unused_assignments)]
#![allow(unused_variables)]
#![allow(improper_ctypes)]
use std::{
    sync::{Arc, Mutex}, 
    arch::x86_64::{_mm_clflush, _mm_mfence}, 
};
use std::time::Duration;
use core::ptr::write_volatile;
use vm_memory::{GuestMemoryMmap, GuestMemory, GuestAddress};
use crate::virtio::{
    vpim::dpu_addr_translation::*, 
    utilities::types::{dpu_rank_t, U64Ptr, U8Ptr,xferPageTablePtr}, 
    request_handlers::xfer_types::{dpu_transfer_mram_address, xfer_page_table, dpu_transfer_mram_batch, SZ_4K}
};
use super::utilities::*;

pub(crate) static mut NB_DPUS_PER_THREAD: u8 = 0; 
pub(crate) static mut NB_THREADS_FOR_XFER: u8 = 0;

use std::thread;
use utils::eventfd::EventFd;
use crate::virtio::vpim::Error as VPIMDeviceError;
use std::time::Instant;

fn signal_used_queue_alt(irq_evt: & EventFd) -> Result<(), VPIMDeviceError>  {
    //println!("Signal");
    irq_evt.write(1).map_err(|e| {
        println!("Error signaling the queue");
        VPIMDeviceError::InterruptError(e)
    })

}

pub(crate) struct xfer_meta{
    pub xfer_matrix : Arc<Box<dpu_transfer_mram_address>>,
    pub arcBase : Arc<Mutex<U64Ptr>>,
    pub mem : GuestMemoryMmap,
    pub direction : thread_mram_xfer,
    pub irq_evt: EventFd
}

pub(crate) struct xfer_meta_batch{
    pub xfer_matrices : Arc<Box<dpu_transfer_mram_batch>>,
    pub arcBase : Arc<Mutex<U64Ptr>>,
    pub mem : GuestMemoryMmap,
    pub direction : thread_mram_xfer,
    pub irq_evt: EventFd
}

pub fn write_to_rank_batch_meta( rank: Arc<Mutex<dpu_rank_t>>, transfer_matrix: Box<dpu_transfer_mram_batch>, mem : &GuestMemoryMmap,irq_evt: &EventFd) -> Result<(), &'static str> {
    let rank_clone = Arc::clone(&rank);
    let mut _rank = &mut *rank_clone.lock().unwrap();
    let mmap = _rank.mmap.as_mut_ptr_range();
    let xfer = xfer_meta_batch{
        xfer_matrices : Arc::new(transfer_matrix),
        arcBase : Arc::new(Mutex::new(U64Ptr(mmap.start as *mut u64))),
        mem : mem.clone(),
        direction : thread_mram_xfer::ThreadMramXferWrite,
        irq_evt: irq_evt.try_clone().unwrap()
    };
    thread::spawn(move || {
        unsafe {
            threads_write_to_rank_batch_meta(Arc::new(& xfer));
            signal_used_queue_alt(&xfer.irq_evt);
            //println!("Task completed,  time : {:#?}", Instant::now());
        }
    });
    Ok(())
}

pub fn write_to_rank_meta(rank: Arc<Mutex<dpu_rank_t>>, transfer_matrix: Box<dpu_transfer_mram_address>, mem : &GuestMemoryMmap, irq_evt: &EventFd) -> Result<(), &'static str> {
    let rank_clone = Arc::clone(&rank);
    let mut _rank = &mut *rank_clone.lock().unwrap();
    let mmap: std::ops::Range<*mut u8> = _rank.mmap.as_mut_ptr_range();
    let xfer = xfer_meta{
        xfer_matrix : Arc::new(transfer_matrix),
        arcBase : Arc::new(Mutex::new(U64Ptr(mmap.start as *mut u64))),
        mem : mem.clone(),
        direction : thread_mram_xfer::ThreadMramXferWrite,
        irq_evt: irq_evt.try_clone().unwrap()
    };
    thread::spawn(move || {
        unsafe {
            //let start = Instant::now();
            threads_write_to_rank_meta(Arc::new(& xfer));
            /* let duration = start.elapsed();
            println!("Time elapsed in write_to_rank_meta() is: {:?}", duration); */
            signal_used_queue_alt(&xfer.irq_evt);
            
            //println!("Task completed,  time : {:#?}", Instant::now());
        }
    });
    Ok(())
}
   /// Read from rank takes the address translation, the base_region_addr, the channel_id, the dpu_transfer_matrix as arguments
   pub fn read_from_rank_meta(rank: Arc<Mutex<dpu_rank_t>>, transfer_matrix :  Box<dpu_transfer_mram_address>, mem : &GuestMemoryMmap,irq_evt: &EventFd) -> Result<(), &'static str> {    
    let rank_clone = Arc::clone(&rank);
    let mut _rank = &mut *rank_clone.lock().unwrap();
    let mmap = _rank.mmap.as_mut_ptr_range();
    let xfer = xfer_meta{
        xfer_matrix : Arc::new(transfer_matrix),
        arcBase : Arc::new(Mutex::new(U64Ptr(mmap.start as *mut u64))),
        mem : mem.clone(),
        direction : thread_mram_xfer::ThreadMramXferRead,
        irq_evt: irq_evt.try_clone().unwrap()
    };
    thread::spawn(move || {
        unsafe {
            //let start = Instant::now();
            threads_read_from_rank_meta(Arc::new(& xfer));
            signal_used_queue_alt(&xfer.irq_evt);
            //let duration = start.elapsed();
            //println!("Time elapsed in read_from_rank_meta() is: {:?}", duration);
            //println!("Task completed,  time : {:#?}", Instant::now());
        }
    });
    Ok(())
}
unsafe fn threads_write_to_rank_batch_meta(xfer: Arc<&xfer_meta_batch>) { 
    //println!("[FIRECRACKER] : START WRITE BATCH");
    crossbeam::thread::scope(|s|{
       let nr_threads = NB_REAL_CIS_U8;
       for i in 0..nr_threads {
           let tid = i;
           let xfer_clone = Arc::clone(&xfer);
           s.spawn(
              move |_| {                    
               let xfer_matrices  = xfer_clone.xfer_matrices.as_ref();
               let mem = &xfer_clone.mem;

                   let id_start = tid;
                   //let id_stop = tid+1;
                   let idx : u8 = id_start * NB_REAL_CIS_U8; 
                   let nb_cis : usize = NB_REAL_CIS;
                   let batch: u8 = 0;
                    let lol = 0;
                   for batch in 0..xfer_matrices.nb_matrices as usize{
                        for ci_id in 0..nb_cis {
                            let xferp = &xfer_matrices.matrix[batch]
                                .ptr[idx as usize + ci_id];
                            if xferp.nb_pages == 0 {
                                continue;
                            }
                            for page in 0..xferp.nb_pages {
                                //println!("[FIRECRACKER] page {} has address {}",page,*xferp.pages.0.add(page as usize));
                                *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                                /*   for lol in 0..50{
                                    print!("{} ", *U8Ptr(*xferp.pages.0.add(page as usize) as *mut u8).0.add(lol));
                                }  
                                println!("Done for page"); */

                            }
                        }
                    
                    }
               }
           );

       }
       
   }).unwrap();
   //let middle = Instant::now();
   //println!("Weare here");
    crossbeam::thread::scope(|s|{

       let nr_threads = NB_THREADS; 
       for i in 0..nr_threads {
           let tid = i;
           let xfer_clone = Arc::clone(&xfer);

           s.spawn(
              move |_| {                    
                let xfer_matrices = xfer_clone.xfer_matrices.as_ref();
                let mem = &xfer_clone.mem;
                let base_region_addr = xfer_clone.arcBase.clone();
                let mut id_start = tid;
               let mut id_stop = tid+1;

               match NB_THREADS {
                   8 => id_start = tid,
                   4 => id_start = tid*2,
                   2 => id_start = tid*4,
                   _ => panic!("Unexpected number of threads")
               };
               match NB_THREADS {
                   8 => id_stop = tid+1,
                   4 => id_stop = tid*2+2,
                   2 => id_stop = tid*4+4,
                   _ => panic!("Unexpected number of threads")
               };
                   let nb_cis : usize = NB_REAL_CIS;
                   let batch :usize = 0;

                   for batch in 0..xfer_matrices.nb_matrices as usize{
                        let mut idx : u8 = id_start * NB_REAL_CIS_U8; 

                        for dpu_id in  id_start..id_stop {
                        
                            let ptr_dest;
                            {
                                ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                            }
                            //println!("HERE WE GO IN MAIN CONTENT : BATCH ID {} AND TID {}", batch,dpu_id );
                            //println!("THE INDEX {} BATCH ID {} AND TID {}", idx, batch,dpu_id);
                            let res = c_write_to_dpus(ptr_dest
                                ,xferPageTablePtr(xfer_matrices.matrix[batch].ptr.as_ptr() as *mut xfer_page_table), xfer_matrices.matrix[batch].size, xfer_matrices.matrix[batch].offset_in_mram, idx);        

                            if res == 4 {
                                idx += NB_REAL_CIS_U8;
                                //println!("HERE GOOOOUUUUUUUDDEEE : BATCH ID {} AND TID {}", batch,dpu_id );
                                continue;
                            }
                            _mm_mfence();
                
                            idx += NB_REAL_CIS_U8;
                            //println!("HERE LEAVE MAIN CONTENT : BATCH ID {} AND TID {}", batch,dpu_id );

                        }
                        //println!("HERE LEAVE BATCH ID {}", batch);

                   }
                   //println!("DONE FUNCTION {}", batch);

               }
           );

       }
       
   }).unwrap();
   //println!("[FIRECRACKER] : END WRITE BATCH");
}   

unsafe fn threads_read_from_rank_meta(xfer: Arc<&xfer_meta>) {
    //Get the xfer matrix first

    crossbeam::thread::scope(|s|{

        let nr_threads = NB_REAL_CIS_U8;
        
        for i in 0..nr_threads {
            let tid = i;
            let xfer_clone = Arc::clone(&xfer);            
            s.spawn(
               move |_| {                    
                let xfer_matrix: &Box<dpu_transfer_mram_address> = xfer_clone.xfer_matrix.as_ref();
                let mem = &xfer_clone.mem;
                let id_start = tid;
                    let id_stop = tid+1;
                    let  idx : u8 = id_start * NB_REAL_CIS_U8; 
                    let nb_cis : usize = NB_REAL_CIS;

                    for ci_id in 0..nb_cis {
                        //Here is the xfer_pages for the dpu idx of the CI of index ci_id
                        let xferp = &xfer_matrix.ptr[idx as usize + ci_id];
                        if xferp.nb_pages == 0 {
                            continue;
                        }
                        for page in 0..xferp.nb_pages {
                            *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                        }
                    }
                }
            );

        }
        
    }).unwrap();

       crossbeam::thread::scope(|s|{
        
        let nr_threads = NB_THREADS;
        for i in 0..nr_threads {
            let tid = i;
            let xfer_clone = Arc::clone(&xfer);
            s.spawn(
               move |_| {
                let xfer_matrix: &Box<dpu_transfer_mram_address> = xfer_clone.xfer_matrix.as_ref();
                let mem = &xfer_clone.mem;
                let base_region_addr = xfer_clone.arcBase.clone();
                let mut id_start = tid;
                let mut id_stop = tid+1;

                match NB_THREADS {
                    8 => id_start = tid,
                    4 => id_start = tid*2,
                    2 => id_start = tid*4,
                    _ => panic!("Unexpected number of threads")
                };
                match NB_THREADS {
                    8 => id_stop = tid+1,
                    4 => id_stop = tid*2+2,
                    2 => id_stop = tid*4+4,
                    _ => panic!("Unexpected number of threads")
                };
                    let mut idx : u8 = id_start * NB_REAL_CIS_U8; 

                    for dpu_id in  id_start..id_stop {
                        let mut ptr_dest = U8Ptr(0 as *mut u8);
                        {
                            ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                        }
                        //FROM HERE
                        let res = c_read_from_dpus(ptr_dest
                            ,xferPageTablePtr(xfer_matrix.ptr.as_ptr() as *mut xfer_page_table), xfer_matrix.size, xfer_matrix.offset_in_mram, idx);        
                        if res == 4 {
                            idx += NB_REAL_CIS_U8;
                            continue;
                        }
                        _mm_mfence();
            
                        idx += NB_REAL_CIS_U8;
                        //TO HERE
                    }

                }
            );
        }
      
    }).unwrap();

}

unsafe fn threads_write_to_rank_meta(xfer: Arc<&xfer_meta>) {       
    crossbeam::thread::scope(|s|{
        let nr_threads = NB_REAL_CIS_U8;
        for i in 0..nr_threads {
            let tid = i;
            let xfer_clone = Arc::clone(&xfer);
            s.spawn(
                move |_| {           
                    let xfer_matrix: &Box<dpu_transfer_mram_address> = xfer_clone.xfer_matrix.as_ref();
                    let mem = &xfer_clone.mem;
                    let id_start = tid;
                    //let id_stop = tid+1;
                    let idx : u8 = id_start * NB_REAL_CIS_U8; 
                    let nb_cis : usize = NB_REAL_CIS;

                    for ci_id in 0..nb_cis {
                        //Here is the xfer_pages for the dpu idx of the CI of index ci_id
                        let xferp: &xfer_page_table = &xfer_matrix.ptr[idx as usize + ci_id];
                        if xferp.nb_pages == 0 {
                            continue;
                        }
                        for page in 0..xferp.nb_pages {
                            *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                        }
                    }
                }
            );
        }
    }).unwrap();
    //println!("[FIRECRACKER] middle");
    crossbeam::thread::scope(|s|{
        let nr_threads = NB_THREADS; 
        for i in 0..nr_threads {
            let xfer_clone = Arc::clone(&xfer);
            let tid = i;
            s.spawn(
               move |_| {                    
                let xfer_matrix: &Box<dpu_transfer_mram_address> = xfer_clone.xfer_matrix.as_ref();
                let mem = &xfer_clone.mem;
                let base_region_addr = xfer_clone.arcBase.clone();
                let mut id_start = tid;
                let mut id_stop = tid+1;

                match NB_THREADS {
                    8 => id_start = tid,
                    4 => id_start = tid*2,
                    2 => id_start = tid*4,
                    _ => panic!("Unexpected number of threads")
                };
                match NB_THREADS {
                    8 => id_stop = tid+1,
                    4 => id_stop = tid*2+2,
                    2 => id_stop = tid*4+4,
                    _ => panic!("Unexpected number of threads")
                };
                    let mut idx : u8 = id_start * NB_REAL_CIS_U8; 
                    let nb_cis : usize = NB_REAL_CIS;
                    for dpu_id in  id_start..id_stop {
                        
                        let ptr_dest;
                        {
                            ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                        }
                        let res = c_write_to_dpus(ptr_dest
                            ,xferPageTablePtr(xfer_matrix.ptr.as_ptr() as *mut xfer_page_table), xfer_matrix.size, xfer_matrix.offset_in_mram, idx);        

                        if res == 4 {
                            idx += NB_REAL_CIS_U8;
                            continue;
                        }
                        _mm_mfence();
            
                        idx += NB_REAL_CIS_U8;
                    }
                }
            );

        }
        
    }).unwrap();

}


 //implement the trait RankOps for the address translation
impl DpuRegionAddressTranslation {
    pub fn init_rank(&mut self,  rank: &mut dpu_rank_t) -> Result<i32, &'static str> {
        self.interleave = DpuRegionInterleaving{
            nb_ci: 8,
            nb_dpus_per_ci: 8,
            mram_size: 64 * 1024 * 1024
        };
        //Set configuration
        self.xfer_thread_conf.nb_threads_per_pool = MAX_THREAD_PER_POOL;
        self.xfer_thread_conf.threshold_1_threads = 1024;
        self.xfer_thread_conf.threshold_2_threads = 2048;
        self.xfer_thread_conf.threshold_4_threads = 32*1024;
        //If channel id not valid, get channel anyway
        //As we only handle one rank here, we don't need arrays of pools. we just need one pool there. 
        //let mut _rank = &mut * Arc::clone(&rank).lock().unwrap();
        rank.internal_data = xeon_sp_private { 
            direction: thread_mram_xfer::ThreadMramXferWrite, 
            xfer_matrix: None, 
            nb_dpus_per_ci: self.interleave.nb_dpus_per_ci, 
            nb_threads_for_xfer: self.xfer_thread_conf.nb_threads_per_pool, 
            nb_threads: self.xfer_thread_conf.nb_threads_per_pool, 
            stop_thread: false,
            xfer_matrices: None,
         };
        unsafe {
            NB_DPUS_PER_THREAD = rank.internal_data.nb_dpus_per_ci / rank.internal_data.nb_threads_for_xfer;
            NB_THREADS_FOR_XFER = rank.internal_data.nb_threads_for_xfer;

        }
        Ok(0)
    }

    pub fn destroy_rank(&mut self, _rank: Arc<Mutex<dpu_rank_t>>) -> Result<(), &'static str> {
        //TODO is there something to do here? maybe unmap the rank only?
        Ok(())
    }

    /// write to control interface and takes the address translation, the base_region_addr, the channel_id, the block data and block size as arguments   
    pub fn write_to_cis(&mut self, rank: Arc<Mutex<dpu_rank_t>>, data: *const u64) -> Result<(), &'static str> {
        //get the raw pointer to the rank.mmap element
        let rank_clone: Arc<Mutex<dpu_rank_t>> = Arc::clone(&rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();
        let mmap = _rank.mmap.as_mut_ptr_range() ;        
        let base_region_addr = mmap.start as *mut u64;
        unsafe{
            let ci_address = ((base_region_addr as *mut u8).add(0x20000)) as *mut u64;
            byte_interleave_avx512(U64Ptr::fromConstPtr(data), U64Ptr(ci_address),TRUE);   
        }    
        self.one_read = false;
        Ok(())
    }

    ///Read from control interface and takes the address translation, the base_region_addr, the channel_id, the block data and block size as arguments
    pub fn read_from_cis(&mut self, rank: Arc<Mutex<dpu_rank_t>>, data: *mut u64) -> Result<(), &'static str> {
        let nb_reads: usize = if self.one_read {2} else {3};
        let mut input: [u64; NB_ELEM_MATRIX] = [0u64;8];
       
        //Getting the base address.
        let rank_clone = Arc::clone(&rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();
        let mmap = _rank.mmap.as_mut_ptr_range();
        let base_region_addr = mmap.start as *mut u64;
        unsafe{
            //getting the ci reading address
            let ci_address = ((base_region_addr as *mut u8).add(0x20000 + 32 * 1024)) as *mut u64;
            //let input: *mut u64 = ci_address;
            //check_boundaries(ci_address, mmap.end as *mut u64);
            for _i in 0..nb_reads {
                //do clflush here
                _mm_clflush(ci_address as *mut u8);
                //do mfence here
                _mm_mfence();
                write_volatile(input.as_mut_ptr().add(0), *(ci_address.offset(0)));
                write_volatile(input.as_mut_ptr().add(1), *(ci_address.offset(1)));
                write_volatile(input.as_mut_ptr().add(2), *(ci_address.offset(2)));
                write_volatile(input.as_mut_ptr().add(3), *(ci_address.offset(3)));
                write_volatile(input.as_mut_ptr().add(4), *(ci_address.offset(4)));
                write_volatile(input.as_mut_ptr().add(5), *(ci_address.offset(5)));
                write_volatile(input.as_mut_ptr().add(6), *(ci_address.offset(6)));
                write_volatile(input.as_mut_ptr().add(7), *(ci_address.offset(7)));           
            }      
            byte_interleave_avx512(U64Ptr::fromArrayU64(&mut input), U64Ptr(data),FALSE);   
            

            self.one_read = true;
 
            Ok(())
        }
    }


/* block_data points to differents objects depending on 'where' the
  * backend is implemented:
  * - in userspace, it points to a virtually contiguous buffer
  * - in kernelspace, it points to an array of pages of size PAGE_SIZE.
  */
 /// Write to rank takes the address translation, the base_region_addr, the channel_id, the dpu_transfer_matrix as arguments
    pub fn write_to_rank(&mut self,  rank: Arc<Mutex<dpu_rank_t>>, transfer_matrix: Box<dpu_transfer_mram_address>, mem : &GuestMemoryMmap) -> Result<(), &'static str> {
        let rank_clone = Arc::clone(&rank);

        let mut _rank = &mut *rank_clone.lock().unwrap();
        let mmap = _rank.mmap.as_mut_ptr_range();
        
        _rank.internal_data.direction = thread_mram_xfer::ThreadMramXferWrite;
        _rank.internal_data.xfer_matrix = Some(transfer_matrix);

        unsafe{
            self.thread_mram(0, mmap.start as *mut u64, mmap.end as *mut u64, _rank, mem);
        }
        
        Ok(())
    }

     
    /// Write to rank takes the address translation, the base_region_addr, the channel_id, the dpu_transfer_matrix as arguments
    pub fn write_to_rank_batch(&mut self,  rank: Arc<Mutex<dpu_rank_t>>, transfer_matrix: Box<dpu_transfer_mram_batch>, mem : &GuestMemoryMmap) -> Result<(), &'static str> {
        let rank_clone = Arc::clone(&rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();
        let mmap = _rank.mmap.as_mut_ptr_range();
        
        _rank.internal_data.direction = thread_mram_xfer::ThreadMramXferWrite;
        _rank.internal_data.xfer_matrices = Some(transfer_matrix);

        unsafe{
            self.thread_mram_batch(0, mmap.start as *mut u64, mmap.end as *mut u64, _rank, mem);
        }
        Ok(())
    }
    
    /// Read from rank takes the address translation, the base_region_addr, the channel_id, the dpu_transfer_matrix as arguments
    pub fn read_from_rank(&mut self,  rank: Arc<Mutex<dpu_rank_t>>, xfer_matrix :  Box<dpu_transfer_mram_address>, mem : &GuestMemoryMmap) -> Result<(), &'static str> {    
        let rank_clone = Arc::clone(&rank);
        let mut _rank = &mut *rank_clone.lock().unwrap();
        let mmap = _rank.mmap.as_mut_ptr_range();
        
        _rank.internal_data.direction = thread_mram_xfer::ThreadMramXferRead;
        _rank.internal_data.xfer_matrix = Some(xfer_matrix);
       
        unsafe{
            self.thread_mram(0, mmap.start as *mut u64, mmap.end as *mut u64, _rank, mem);
        }
        Ok(())
    }

    pub unsafe fn thread_mram(&self , _thread_id : u8, base_region_addr: *mut u64, end: *mut u64, rank : &mut dpu_rank_t, mem : &GuestMemoryMmap){
        let basePtr =  U64Ptr(base_region_addr);
        let arcBase = Arc::new(Mutex::new(basePtr));
        let _rank = Arc::new(rank);

        if _rank.internal_data.direction == thread_mram_xfer::ThreadMramXferWrite {
            self.threads_write_to_rank( arcBase , _rank, mem);
        } else {
            self.threads_read_from_rank(arcBase, _rank, mem);
        }
       
    }   
    
    pub unsafe fn thread_mram_batch(&self , _thread_id : u8, base_region_addr: *mut u64, end: *mut u64, rank : &mut dpu_rank_t, mem : &GuestMemoryMmap){
        let basePtr =  U64Ptr(base_region_addr);
        let arcBase = Arc::new(Mutex::new(basePtr));
        let _rank = Arc::new(rank);

        if _rank.internal_data.direction == thread_mram_xfer::ThreadMramXferWrite {
            self.threads_write_to_rank_batch( arcBase , _rank, mem);
        } else {
            self.threads_read_from_rank(arcBase, _rank, mem);
        }
       
    } 

    pub unsafe fn threads_write_to_rank(&self, base_region_addre: Arc<Mutex<U64Ptr>>,  rank : Arc<&mut dpu_rank_t>, mem : &GuestMemoryMmap) {         
        crossbeam::thread::scope(|s|{
            let nr_threads = NB_REAL_CIS_U8;
            for i in 0..nr_threads {
                let tid = i;
                let _rank1 = rank.clone();
                s.spawn(
                    move |_| {                    
                    let xfer_matrix  = _rank1.internal_data.xfer_matrix.as_ref().unwrap();
                        let id_start = tid;
                        //let id_stop = tid+1;
                        let idx : u8 = id_start * NB_REAL_CIS_U8; 
                        let nb_cis : usize = NB_REAL_CIS;

                        for ci_id in 0..nb_cis {
                            //Here is the xfer_pages for the dpu idx of the CI of index ci_id
                            let xferp: &xfer_page_table = &xfer_matrix.ptr[idx as usize + ci_id];
                            if xferp.nb_pages == 0 {
                                continue;
                            }
                            for page in 0..xferp.nb_pages {
                                *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                            }
                        }
                    }
                );
            }
        }).unwrap();
        //let middle = Instant::now();
        crossbeam::thread::scope(|s|{
            let nr_threads = NB_THREADS; 
            for i in 0..nr_threads {
                let tid = i;
                let _rank1 = rank.clone();
                let base_region_addr = base_region_addre.clone();
                s.spawn(
                   move |_| {                    
                    
                    let xfer_matrix  = _rank1.internal_data.xfer_matrix.as_ref().unwrap();
                    let mut id_start = tid;
                    let mut id_stop = tid+1;

                    match NB_THREADS {
                        8 => id_start = tid,
                        4 => id_start = tid*2,
                        2 => id_start = tid*4,
                        _ => panic!("Unexpected number of threads")
                    };
                    match NB_THREADS {
                        8 => id_stop = tid+1,
                        4 => id_stop = tid*2+2,
                        2 => id_stop = tid*4+4,
                        _ => panic!("Unexpected number of threads")
                    };
                        let mut idx : u8 = id_start * NB_REAL_CIS_U8; 
                        let nb_cis : usize = NB_REAL_CIS;

                       
                        for dpu_id in  id_start..id_stop {
                            
                            let ptr_dest;
                            {
                                ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                            }
                            let res = c_write_to_dpus(ptr_dest
                                ,xferPageTablePtr(xfer_matrix.ptr.as_ptr() as *mut xfer_page_table), xfer_matrix.size, xfer_matrix.offset_in_mram, idx);        

                            if res == 4 {
                                idx += NB_REAL_CIS_U8;
                                continue;
                            }
                            _mm_mfence();
                
                            idx += NB_REAL_CIS_U8;
                        }
                    }
                );

            }
            
        }).unwrap();
        
    }   
 
    
    pub unsafe fn threads_write_to_rank_batch(&self, base_region_addre: Arc<Mutex<U64Ptr>>,  rank : Arc<&mut dpu_rank_t>, mem : &GuestMemoryMmap) { 
        //println!("[FIRECRACKER] : START WRITE BATCH");
        crossbeam::thread::scope(|s|{
           let nr_threads = NB_REAL_CIS_U8;
           for i in 0..nr_threads {
               let tid = i;
               let _rank1 = rank.clone();
               s.spawn(
                  move |_| {                    
                   let xfer_matrices  = _rank1.internal_data.xfer_matrices.as_ref().unwrap();
                       let id_start = tid;
                       //let id_stop = tid+1;
                       let idx : u8 = id_start * NB_REAL_CIS_U8; 
                       let nb_cis : usize = NB_REAL_CIS;
                       let batch: u8 = 0;
                        let lol = 0;
                       for batch in 0..xfer_matrices.nb_matrices as usize{
                            for ci_id in 0..nb_cis {
                                let xferp = &xfer_matrices.matrix[batch]
                                    .ptr[idx as usize + ci_id];
                                if xferp.nb_pages == 0 {
                                    continue;
                                }
                                for page in 0..xferp.nb_pages {
                                    //println!("[FIRECRACKER] page {} has address {}",page,*xferp.pages.0.add(page as usize));
                                    *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                                    /*   for lol in 0..50{
                                        print!("{} ", *U8Ptr(*xferp.pages.0.add(page as usize) as *mut u8).0.add(lol));
                                    }  
                                    println!("Done for page"); */

                                }
                            }
                        
                        }
                   }
               );

           }
           
       }).unwrap();
       //let middle = Instant::now();
       //println!("Weare here");
        crossbeam::thread::scope(|s|{

           let nr_threads = NB_THREADS; 
           for i in 0..nr_threads {
               let tid = i;
               let _rank1 = rank.clone();
               let base_region_addr = base_region_addre.clone();
               s.spawn(
                  move |_| {                    
                   
                    let xfer_matrices  = _rank1.internal_data.xfer_matrices.as_ref().unwrap();
                    let mut id_start = tid;
                   let mut id_stop = tid+1;

                   match NB_THREADS {
                       8 => id_start = tid,
                       4 => id_start = tid*2,
                       2 => id_start = tid*4,
                       _ => panic!("Unexpected number of threads")
                   };
                   match NB_THREADS {
                       8 => id_stop = tid+1,
                       4 => id_stop = tid*2+2,
                       2 => id_stop = tid*4+4,
                       _ => panic!("Unexpected number of threads")
                   };
                       let nb_cis : usize = NB_REAL_CIS;
                       let batch :usize = 0;

                       for batch in 0..xfer_matrices.nb_matrices as usize{
                            let mut idx : u8 = id_start * NB_REAL_CIS_U8; 

                            for dpu_id in  id_start..id_stop {
                            
                                let ptr_dest;
                                {
                                    ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                                }
                                //println!("HERE WE GO IN MAIN CONTENT : BATCH ID {} AND TID {}", batch,dpu_id );
                                //println!("THE INDEX {} BATCH ID {} AND TID {}", idx, batch,dpu_id);
                                let res = c_write_to_dpus(ptr_dest
                                    ,xferPageTablePtr(xfer_matrices.matrix[batch].ptr.as_ptr() as *mut xfer_page_table), xfer_matrices.matrix[batch].size, xfer_matrices.matrix[batch].offset_in_mram, idx);        
    
                                if res == 4 {
                                    idx += NB_REAL_CIS_U8;
                                    //println!("HERE GOOOOUUUUUUUDDEEE : BATCH ID {} AND TID {}", batch,dpu_id );
                                    continue;
                                }
                                _mm_mfence();
                    
                                idx += NB_REAL_CIS_U8;
                                //println!("HERE LEAVE MAIN CONTENT : BATCH ID {} AND TID {}", batch,dpu_id );

                            }
                            //println!("HERE LEAVE BATCH ID {}", batch);

                       }
                       //println!("DONE FUNCTION {}", batch);

                   }
               );

           }
           
       }).unwrap();
       //println!("[FIRECRACKER] : END WRITE BATCH");
    }   
   

    pub unsafe fn threads_read_from_rank(&self, base_region_addre: Arc<Mutex<U64Ptr>>, rank : Arc<&mut dpu_rank_t>, mem : &GuestMemoryMmap) {
        //Get the xfer matrix first

        crossbeam::thread::scope(|s|{

            let nr_threads = NB_REAL_CIS_U8;
            
            for i in 0..nr_threads {
                let tid = i;
                let _rank1 = rank.clone();
                s.spawn(
                   move |_| {                    
                    let xfer_matrix  = _rank1.internal_data.xfer_matrix.as_ref().unwrap();
                        let id_start = tid;
                        let id_stop = tid+1;
                        let  idx : u8 = id_start * NB_REAL_CIS_U8; 
                        let nb_cis : usize = NB_REAL_CIS;

                        for ci_id in 0..nb_cis {
                            //Here is the xfer_pages for the dpu idx of the CI of index ci_id
                            let xferp = &xfer_matrix.ptr[idx as usize + ci_id];
                            if xferp.nb_pages == 0 {
                                continue;
                            }
                            for page in 0..xferp.nb_pages {
                                *xferp.pages.0.add(page as usize) = U8Ptr(mem.get_slice(GuestAddress(*(xferp.pages.0.add(page as usize))), SZ_4K as usize).unwrap().as_ptr()).toU64();
                            }
                        }
                    }
                );

            }
            
        }).unwrap();

           crossbeam::thread::scope(|s|{
            
            let nr_threads = NB_THREADS;
            for i in 0..nr_threads {
                let tid = i;
                let _rank1 = rank.clone();
                let base_region_addr = base_region_addre.clone();
                s.spawn(
                   move |_| {
                    let xfer_matrix  = _rank1.internal_data.xfer_matrix.as_ref().unwrap();
                    let mut id_start = tid;
                    let mut id_stop = tid+1;

                    match NB_THREADS {
                        8 => id_start = tid,
                        4 => id_start = tid*2,
                        2 => id_start = tid*4,
                        _ => panic!("Unexpected number of threads")
                    };
                    match NB_THREADS {
                        8 => id_stop = tid+1,
                        4 => id_stop = tid*2+2,
                        2 => id_stop = tid*4+4,
                        _ => panic!("Unexpected number of threads")
                    };
                        let mut idx : u8 = id_start * NB_REAL_CIS_U8; 
    
                        for dpu_id in  id_start..id_stop {
                            let mut ptr_dest = U8Ptr(0 as *mut u8);
                            {
                                ptr_dest = U8Ptr((base_region_addr.lock().unwrap().toU8Ptr().0).add(0x40000 * (dpu_id as usize % 4) + (if dpu_id as usize >= 4 {0x40} else {0})));
                            }
                            //FROM HERE
                            let res = c_read_from_dpus(ptr_dest
                                ,xferPageTablePtr(xfer_matrix.ptr.as_ptr() as *mut xfer_page_table), xfer_matrix.size, xfer_matrix.offset_in_mram, idx);        
                            if res == 4 {
                                idx += NB_REAL_CIS_U8;
                                continue;
                            }
                            _mm_mfence();
                
                            idx += NB_REAL_CIS_U8;
                            //TO HERE
                        }
    
                    }
                );
            }
          
        }).unwrap();

    }
}