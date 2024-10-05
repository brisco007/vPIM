fn main() {
    println!("cargo:rustc-link-search=native=/firecracker/src/devices/src/clib"); // +
    println!("cargo:rustc-link-lib=static=helloworld");
    println!("cargo:rustc-link-lib=static=avx512");
}