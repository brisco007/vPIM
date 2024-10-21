# vPIM - Virtualization of Processing-In-Memory

This repository contains the code for PIM hardware virtualization, organized into two folders: **apps** and **code**.

## APPS Folder
This folder contains utility code:
- **pin.sh**: Used to pin programs to CPUs.
- **sdkAction**: Used to pull and compile the SDK from the repository: https://github.com/852866031/upmem-sdk.git. However, a version of the code is already included, so you may skip the cloning step.
- **Debian packages**: Includes the following packages:
  - *upmem_2021.3.0_amd64.deb*: Installs version **2021.3.0** of the UPMEM SDK.
  - *upmem-driver-dkms_2021.3.0_amd64.deb*: Installs the kernel driver for UPMEM hardware.
- **dpu_demo**: A checksum application. Find the original repository and instructions here: https://github.com/upmem/dpu_demo.git
- **PrIM benchmark**: Code for the PrIM benchmark. The original repository with usage instructions can be found here: https://github.com/CMU-SAFARI/prim-benchmarks.git. To simplify running the benchmarks, we have provided Python scripts located at the root folder: *run_strong_full*, *run_strong_rank.py*, and *run_weak*.
- **vPrIM**: This was developed to improve the performance of PrIM applications, primarily by reducing the number of requests. Each app has specific updates.
- **Miscellaneous**: The *upmem-code* folder contains various code snippets, such as microbenchmarks for UPMEM API functions and a "hello world" example.

## CODE Folder
- **Firecracker Installation**: You'll need the *Rust* compiler and the musl toolchain to compile and use Firecracker. The Docker engine is also required. Once the environment is set up, compile Firecracker using:
  ```
  sudo ./tools/devtool build
  ```
- **Linux Kernel (version 5.10.98)**: This folder contains the kernel compiled with our vPIM driver, located at *guest-5.10.98/drivers/virtio/virtio_pim*.
- **vPIM Manager**: This should be launched before Firecracker, as it allocates ranks to Firecracker.
- **Scripts**:
  - Use *nsfc-set-socket.sh* to start a Firecracker instance.
  - Use *run-firecracker.sh* to send requests for VM creation. The version with a "2" suffix can be used to launch a second VM. Be sure to modify it to avoid conflicts, such as incorrect device counts.
- **vpim_driver_setup.tar.xz**: Use the installation script inside this file to add virtual PIM devices to the plugdev group in the VM. Copy this file into your VM disk beforehand.

## BEFORE RUNNING OUR SYSTEM
We use Firecracker as the VM. First, build a root filesystem by following this [tutorial](https://happybear.medium.com/building-ubuntu-20-04-root-filesystem-for-firecracker-e3f4267e58cc). Note that you should allocate at least 8GB for the virtual disk.

Once the disk is created, mount it:
```
sudo mount <my_disk> /mnt/<my_folder>
```
Copy the `apps` folder and the `vpim_driver_setup.tar.xz` file to the virtual machine disk:
```
sudo cp <path to the project root>/apps /mnt/<my_folder>/root/
sudo cp <path to the project root>/code/vpim_driver_setup.tar.xz /mnt/<my_folder>/root/
```
Install the required dependencies in the root filesystem:
```
chroot /mnt/<my_folder>
apt install libffi-dev libgcc-8-dev libnuma1 libstdc++-8-dev pkg-config python3 python3-pygments python3-serial libedit-dev
```
The root filesystem is now ready to be used.

## RUN THE MANAGER
To run the manager, navigate to the manager folder:
```
cd code/vpim_manager
```
Compile the manager:
```
make
```
Run the manager:
```
vpim_manager/bin/app
```
The manager will list the available ranks in the system. You can compare this with the output of the `dpu-diag` command to ensure all ranks are listed.

## START THE VIRTUAL MACHINE
To start a virtual machine, run the following scripts:
1. Set up the Firecracker socket:
   ```
   cd code
   sudo ./nsfc-set-socket.sh
   ```
2. In another terminal, send requests for resources to allocate to the VM:
   ```
   sudo ./run-firecracker.sh
   ```
   You can modify this script to change the allocated resources, such as the root filesystem or the number of ranks. For example, to allocate more ranks:
   ```
   curl --unix-socket /tmp/firecracker.socket -i \
     -X PUT 'http://localhost/vpim' \
     -H 'Accept: application/json' \
     -H 'Content-Type: application/json' \
     -d '{
         "hold_rank":1
       }'
   ```
To launch a second virtual machine, use the scripts with a "2" suffix:
```
sudo ./nsfc-set-socket2.sh
sudo ./run-firecracker2.sh
```

## SETUP THE VIRTUAL MACHINE ENVIRONMENT

If this is your first time running the virtual machine, install the UPMEM SDK. Navigate to the `apps` folder in the root filesystem:
```
cd apps
```
Install the SDK package:
```
dpkg -i upmem_2021.3.0_amd64.deb
```
Extract the `vpim_driver_setup.tar.xz` file:
```
cd ..
tar -xvf vpim_driver_setup.tar.xz
cd vpim_driver_setup/
```
Make the setup script executable and run it:
```
chmod +x ./vpim_userspace_setup.sh
sudo ./vpim_userspace_setup.sh
```
Reboot the VM to apply the changes:
```
reboot
```
After rebooting, check if the ranks are visible by running:
```
dpu-diag
```
The output should list the ranks as in the native environment.

## RUN PROGRAMS IN THE VIRTUAL MACHINE
You can run vPrIM examples with one of the provided scripts: `run_vprim.py`, `run_strong_rank.py`, or `run_strong_full.py`. For example:
```
cd apps/vPrIM
python3 run_strong_rank.py VA
```
This will run the VA program from the PrIM benchmark. The full list of applications is available [here](https://github.com/CMU-SAFARI/prim-benchmarks.git).

NOTE : When running applications on UPMEM hardware (in a VM or otherwise), you might encounter a "Wavegen error." This is an internal hardware error. If this occurs, power cycle your server with:
```
sudo ipmitool power cycle
```
If these errors happen in the virtual machine, shut down the VM first and then do the power cycle.
