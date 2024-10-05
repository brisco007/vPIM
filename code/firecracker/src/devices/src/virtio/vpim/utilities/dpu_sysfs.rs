use std::fs::{File, OpenOptions};
use std::io::{Read, self};
use std::mem::size_of;
use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::sync::{Mutex, Arc};
use crate::virtio::request_handlers::xfer_types::{SZ_4K, SZ_1K};
use crate::virtio::vpim::*;
use glob::glob;
use memmap2::MmapMut;
use crate::virtio::vpim::dpu_addr_translation::*;
use super::hardware::{dpu_rank_fs, dpu_description_t, dpu_runtime_state_t};
use super::{types::*, manager};
use super::vpd::dpu_vpd;
    

///Invalid rank index
pub const INVALID_RANK_INDEX: u8 = 255;
//create a public sysfs trait to be used by the dpu_sysfs module

//function that reads a file and returns its content as a template type T
pub fn read_sysfs_data_from_file<T>(path: &Path) -> T where T: std::str::FromStr {
    let mut file = File::open(path).expect("Unable to open file");
    let mut contents = String::new();
    file.read_to_string(&mut contents).expect("Unable to read file");
    match contents.trim().parse::<T>() {
        Ok(x) => x,
        Err(_) => panic!("Unable to parse file {}", path.display())
    }
}


///We are trying to allocate a rank
/// # Arguments
/// * `device rank path` - the path to the rank
/// # Returns
/// * `true` - if the rank is allocated
/// * `false` - if the rank is not allocated
 pub(crate) fn dpu_sysfs_try_to_allocate_rank(response :manager::Response) -> Option<dpu_rank_fs> {
    //normally we check the capability but we know we are in perf mode
    //in the /dev directory, loop on all the files with the pattern dax*
    let mut rank_fs: dpu_rank_fs = dpu_rank_fs::default();
    let path : PathBuf = PathBuf::from_str(response.rank_path.as_str()).unwrap();
    let fdpu_rank = OpenOptions::new().read(true).write(true).open(path.as_path());
    match fdpu_rank {
        Ok(fdpu_rank) => {
            rank_fs.fdpu_rank = Some(fdpu_rank);
            rank_fs.rank_id = response.rank_id as u8;
            rank_fs.rank_path = format!("/sys/class/dpu_rank/dpu_rank{}/", response.rank_id);
            let dax_path = format!("/sys/class/dpu_dax/dax{}.{}/", response.rank_id,response.rank_id);
            rank_fs.dax_path = dax_path;
            rank_fs.fdpu_dax = OpenOptions::new().read(true).write(true).open(PathBuf::from(response.dax_path).as_path()).ok();
            return Some(rank_fs);
        }
        Err(_) => {
            //TODO write an error here
            println!("Unable to open file");
        },
    }
    None
} 

pub(crate) fn init_rank_struct(id:u8) -> Arc<Mutex<dpu_rank_t>> {
    Arc::new(Mutex::new(dpu_rank_t {
        type_: 0,
        rank_id: id,
        description: dpu_description_t::default(),
        runtime: dpu_runtime_state_t::default(),
        dpu_addresses: 0,
        matrix: DpuTransferMatrix {
            ptr: [0; MAX_NR_DPUS_PER_RANK],
            offset: 0,
            size: 0,
        },
        callback_matrix:  DpuTransferMatrix {
            ptr: [0; MAX_NR_DPUS_PER_RANK],
            offset: 0,
            size: 0,
        },
        dpus: vec![dpu_t::default(); MAX_NR_DPUS_PER_RANK],
        nr_dpus_enabled: 0,
        cmds: [0; DPU_MAX_NR_CIS],
        data: [0; DPU_MAX_NR_CIS],
        numa_node: 0,
        channel_id: 0,
        control_interfaces: [0; DPU_MAX_NR_CIS],
        dpu_chip_id: 0,
        nb_cis: 8,
        region_size: 65421,
        mmap: MmapMut::map_anon(1).unwrap(),
        internal_data: xeon_sp_private { 
            direction: thread_mram_xfer::ThreadMramXferRead, 
            xfer_matrix: None, 
            nb_dpus_per_ci: 8, 
            nb_threads_for_xfer: 1, 
            nb_threads: 1, 
            xfer_matrices: None,
            stop_thread: false },
        config_data: dpu_rank_request_props::default(),
        
    }))
}

impl VPIMDevice {
        
    pub fn dpu_sysfs_get_region_size(&mut self) -> usize {
        let filepath : PathBuf =  PathBuf::from(format!("{}size", self.rank_fs.dax_path));
        read_sysfs_data_from_file::<usize>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_channel_id(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}channel_id", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_rank_id(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}rank_id", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_backend_id(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}backend_id", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_mode(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}mode", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_debug_mode(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}debug_mode", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_dpu_chip_id(&mut self)-> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}dpu_chip_id", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_nb_ci(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}nb_ci", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_nb_dpus_per_ci(&mut self)-> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}nb_dpus_per_ci", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_mram_size(&mut self) -> u32 {
        let filepath : PathBuf =  PathBuf::from(format!("{}mram_size", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u32>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_capabilities(&mut self) -> u64 {
        let filepath : PathBuf =  PathBuf::from(format!("{}capabilities", self.rank_fs.rank_path));
        let mut file = File::open(filepath.as_path()).expect("Unable to open file");
        let mut contents = String::new();
        file.read_to_string(&mut contents).expect("Unable to read file");
        //println!("HERE ARE CAPABILITIES {}", contents);
        u64::from_str_radix(contents.trim().trim_start_matches("0x"), 16).unwrap() 
    }
    pub fn dpu_sysfs_get_fck_frequency(&mut self) -> u32 {
        let filepath : PathBuf =  PathBuf::from(format!("{}fck_frequency", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u32>(filepath.as_path())
    }
    //write dpu_sysfs_get_clock_division
    pub fn dpu_sysfs_get_clock_division(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}clock_division", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_is_owned(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}is_owned", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_usage_count(&mut self) -> u32 {
        let filepath : PathBuf =  PathBuf::from(format!("{}usage_count", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u32>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_numa_node(&mut self) -> i32{
        let filepath : PathBuf =  PathBuf::from(format!("{}numa_node", self.rank_fs.dax_path));
        read_sysfs_data_from_file::<i32>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_rank_index(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}rank_index", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_rank_count(&mut self) -> u8 {
        let filepath : PathBuf =  PathBuf::from(format!("{}rank_count", self.rank_fs.rank_path));
        read_sysfs_data_from_file::<u8>(filepath.as_path())
    }
    pub fn dpu_sysfs_get_byte_order(&mut self) -> String {
        let filepath : PathBuf =  PathBuf::from(format!("{}byte_order", self.rank_fs.rank_path));
        let mut file = File::open(filepath.as_path()).expect("Unable to open file");
        let mut contents = String::new();
        file.read_to_string(&mut contents).expect("Unable to read file");
        contents
    }

    //Free rank 
    pub fn dpu_sysfs_free_rank(&mut self) -> () {
        //TODO see if needed
    }

    ///Get the number of physical ranks
    pub fn dpu_sysfs_get_nb_physical_ranks(&mut self) -> u8 {
        let mut nb_rank:u8= 0;
        for _entry in glob("/dev/dax*").expect("Failed to read glob pattern") {
            nb_rank += 1;
        }
        nb_rank
    }

    // We assume there is only one chip id per machine: so we just need to get that info
    // from the first rank.
    ///Get the chip id from the machine

    // We assume the topology is identical for all the ranks: so we just need to get that info
    // from the first rank.
    ///Get the topology from the machine
    pub fn dpu_sysfs_get_hardware_description(&mut self) -> () {
        //GET INFORMATIONS  
        self.description.hw.topology.nr_of_dpus_per_control_interface = self.dpu_sysfs_get_nb_dpus_per_ci();
        self.description.hw.topology.nr_of_control_interfaces = self.dpu_sysfs_get_nb_ci();
        self.description.hw.memories.mram_size = self.dpu_sysfs_get_mram_size();
        self.description.hw.timings.clock_division = self.dpu_sysfs_get_clock_division();
        self.description.hw.timings.fck_frequency_in_mhz = self.dpu_sysfs_get_fck_frequency();
        self.capability_mode = self.dpu_sysfs_get_capabilities();
        self.description.hw.memories.wram_size = 64 * SZ_1K as u32;
        self.description.hw.memories.iram_size = SZ_4K;
        self.description.hw.timings.carousel.cmd_duration = 2;
        self.description.hw.timings.carousel.cmd_sampling = 1;
        self.description.hw.timings.carousel.res_duration = 2;
        self.description.hw.timings.carousel.res_sampling = 1;
        self.description.hw.timings.reset_wait_duration = 20;
        self.description.hw.timings.std_temperature = 110;
        self.description.hw.dpu.nr_of_threads = 24;
        self.description.hw.dpu.nr_of_atomic_bits = 256;
        self.description.hw.dpu.nr_of_notify_bits = 40;
        self.description.hw.dpu.nr_of_work_registers_per_thread = 24;
        self.description.configuration.ignore_vpd = true;
        self.description.configuration.enable_cycle_accurate_behavior = true;
        self.description.configuration.init_mram_mux = true;
        self.description.configuration.disable_reset_on_alloc = true;
        self.description.configuration.api_must_switch_mram_mux = true;
        self.description.configuration.do_iram_repair = false;
        self.description.configuration.do_wram_repair = false;

    }

    pub fn dpu_sysfs_get_kernel_module_version(&mut self) -> (u32,u32) {
        let mut kernel_module_version_file: File = File::open("/sys/module/dpu/version").unwrap();
        let kernel_module_version_file_content;
        let mut perm  = kernel_module_version_file.metadata().unwrap().permissions();
        perm.set_readonly(true);
        match kernel_module_version_file.set_permissions(perm) {
            Ok(_) => {},
            Err(_) => println!("There was an error setting the file permissions in sysfs get module version dpu_sysfs"),
        }
        let buf: &mut [u8] = &mut [0; 128];
        kernel_module_version_file.read(buf).unwrap();
        kernel_module_version_file_content = String::from_utf8_lossy(buf).to_string();
        println!("kernel_module_version_file_content: {}", kernel_module_version_file_content);
        let kernel_module_version_file_content_split: Vec<&str> = kernel_module_version_file_content.split(".").collect();
        let kernel_module_version_major: u32 = kernel_module_version_file_content_split[0].parse::<u32>().unwrap();
        let kernel_module_version_minor: u32 = kernel_module_version_file_content_split[1].parse::<u32>().unwrap();
        return (kernel_module_version_major, kernel_module_version_minor);
    }

    pub fn dpu_sysfs_get_dimm_serial_number(&mut self) -> [u8;10] {
        let mut dimm_serial_number_file: File = File::open(format!("/sys/class/dpu_rank/dpu_rank{}/serial_number",self.rank_fs.rank_id)).unwrap();
        let mut perm  = dimm_serial_number_file.metadata().unwrap().permissions();
        perm.set_readonly(true);
        match dimm_serial_number_file.set_permissions(perm) {
            Ok(_) => {},
            Err(_) => {println!("Problem setting file permissions get dimm serial number")},
        }
        let mut buf:  [u8;10] = [0; 10];
        match dimm_serial_number_file.read(&mut buf) {
            Ok(_) => {},
            Err(_) => {println!("Error reading the dimm S/N")}
        };
        return buf;
    }
    pub fn dpu_sysfs_get_mcu_version(&mut self) ->  [u8;128]  {
        let mut mcu_version_file: File = File::open(format!("/sys/class/dpu_rank/dpu_rank{}/mcu_version",self.rank_fs.rank_id)).unwrap();
        let mut perm  = mcu_version_file.metadata().unwrap().permissions();
        perm.set_readonly(true);
        match mcu_version_file.set_permissions(perm) {
            Ok(_) => {},
            Err(_) => {println!("Problem setting file permissions get MCU version")},
        }
        let mut buf:  [u8;128] = [0; 128];
        match mcu_version_file.read(&mut buf) {
            Ok(_) => {},
            Err(_) => {println!("Error reading the MCU version")}
        };
        return buf;
    }
    pub fn dpu_sysfs_get_part_number(&mut self) ->  [u8;20]  {
        let mut part_number_file: File = File::open(format!("/sys/class/dpu_rank/dpu_rank{}/part_number",self.rank_fs.rank_id)).unwrap();
        let mut perm  = part_number_file.metadata().unwrap().permissions();
        perm.set_readonly(true);
        match part_number_file.set_permissions(perm) {
            Ok(_) => {},
            Err(_) => {println!("Problem setting file permissions get dimm part number")},
        }
        let mut buf:  [u8;20] = [0; 20];
        match part_number_file.read(&mut buf) {
            Ok(_) => {
            },
            Err(_) => {println!("Error reading the dimm part number")}
        };
        return buf;
    }

    pub fn dpu_sysfs_initvpd(&mut self) -> io::Result<dpu_vpd>  {
        let mut dimm_vpd_file: File = File::open(format!("/sys/class/dpu_rank/dpu_rank{}/dimm_vpd",self.rank_fs.rank_id)).unwrap();
        let mut vpd_ptr : Box<dpu_vpd> = Box::new(dpu_vpd::default()); 
        let mut buf  = [0u8; size_of::<dpu_vpd>()];
        let nbytes = dimm_vpd_file.read(&mut buf);
        unsafe {
            match nbytes {
                Ok(nb) => {
                    // nb is the buffer size. we set the buffer to vpd if the size is smaller.
                    if nb > 0 && nb <= size_of::<dpu_vpd>() {
                        *vpd_ptr =*(buf.as_mut_ptr() as *mut dpu_vpd);
                    }

                    Ok(*vpd_ptr)
                },
                Err(e) => {
                    println!("DIMM VPD ERROR : {:#?}\n", e);
                    Err(e)},
            }   
        }
    }

    pub fn byte_order_parse(&mut self, _rank :&mut dpu_rank_t) -> bool {
        let byte_order = self.dpu_sysfs_get_byte_order();
        let byte_order_split: Vec<&str> = byte_order.trim().split(" ").collect();

        //println!("{}",byte_order_split.len());
        for i in 0..byte_order_split.len() {
            let data = u64::from_str_radix(byte_order_split[i].trim().trim_start_matches("0x"), 16).unwrap();
            _rank.set_ci_slice_info(i, data);
        }
        return true;
    }
}



