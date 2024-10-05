use vm_memory::{GuestMemoryMmap, GuestAddress, Bytes, GuestMemoryError};


#[inline(always)]
#[allow(dead_code)] // Only used on 32-bit builds currently
pub fn u64_as_u8<'a>(src: &'a [u64]) -> &'a [u8] {
    unsafe {
        core::slice::from_raw_parts(src.as_ptr() as *mut u8, src.len() * 8)
    }
}
#[inline(always)]
#[allow(dead_code)] // Only used on 32-bit builds currently
pub fn u64_as_u8_mut<'a>(src: &'a mut [u64]) -> &'a mut [u8] {
    unsafe {
        core::slice::from_raw_parts_mut(src.as_mut_ptr() as *mut u8, src.len() * 8)
    }
}


/// utility write data (1-bytes blocks) to the guest memory using its GPA 
pub fn memory_write_u8(mem : &GuestMemoryMmap, gpa : u64, buffer: &[u8]) -> Result<(), GuestMemoryError>{
    let addr = GuestAddress(gpa);
    let _ignored = mem.write_slice(buffer, addr);
    Ok(())
}
/// utility write data (8-bytes blocks) to the guest memory using its GPA 
pub fn memory_write_u64(mem : &GuestMemoryMmap, gpa : u64, buffer:  &[u64]) -> Result<(), GuestMemoryError>{
    let addr = GuestAddress(gpa);
    let buf = u64_as_u8(buffer);
    let _ignored = mem.write_slice(buf, addr);
    Ok(())
}
pub fn memory_read_u64(mem : &GuestMemoryMmap, gpa : u64, buffer:  &mut [u64]) -> Result<(), GuestMemoryError>{
    let addr = GuestAddress(gpa);
    let buf = u64_as_u8_mut(buffer);
    let _ignored = mem.read_slice(buf, addr);
    Ok(())
}

/// utility read data (1-bytes blocks) from the guest memory using its GPA 
pub fn memory_read_u8(mem : &GuestMemoryMmap, gpa : u64, buffer: &mut [u8])  -> Result<(), GuestMemoryError>{
    let addr = GuestAddress(gpa);
    let _output = mem.read_slice(buffer, addr);
    Ok(())
}

