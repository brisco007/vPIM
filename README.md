# vPIM - Virtualization of Processing-In-Memory

This repository contains the code for PIM hardware virtualization. 
This folder contains several elements dispatched into 2 folders : **apps** and **code**.

## APPS folder
This conains utilitiy code.
- **pin.sh** : That is used to pin programs to CPUs.
- **sdkAction** is used to pull the sdk and compile it from the repository : https://github.com/852866031/upmem-sdk.git. Though there is a pulled version of the code so you might skip the cloning phase. 
- **Debian packages** : These packages are : *upmem_2021.3.0_amd64.deb* and *upmem-driver-dkms_2021.3.0_amd64.deb*; and they are respectively used to install the  version **2021.3.0** of the SDK and kernel driver for UPMEM hardware. 
- **dpu_demo** : This is a code for a checksum application. Here is the original repository and how to run it : https://github.com/upmem/dpu_demo.git
- **PrIM benchmark** : This is the code for the PrIM benchmark. here is the original repo that presents how to run it : https://github.com/CMU-SAFARI/prim-benchmarks.git. We have written some python scripts that can be found at the root folder, in order to run them more easily. they are : *run_strong_full*, *run_strong_rank.py* and *run_weak*.
- **vPrIM** : This has been developped in order to improve the performance of PrIM applications. Each app has specific updates but most of the modifications were about reducing the number of requests.
- **Miscellanious** : The *upmem-code* folder contains miscellanious code : a micro benchmark for each function available un the UPMEM API, a hello world code, etc. 

## CODE FOLDER
- **This contains the firecracker installation** : you need to install the *Rust* compiler and the musl toolchain in order to compile and use firecracker. it also requires the docker engine to be installed.
Once the environment has been setup, you just need to compile firecracker with : *sudo ./tools/devtool build*
- **This contains the linux kernel (version 5.10.98)** :  compiled with our vPIM driver. you can find that driver at : *guest-5.10.98/drivers/virtio/virtio_pim*.
- **The vpim manager** : that should be launched before launching firecracker for he is the one that shall allocate ranks to firecracker.
- **Scripts** : use the *nsfc-set-socket.sh* to run one firecracker instance and *run-firecracker.sh* to send requests to create the VM. use the version with suffix 2 in order to launch a second VM if needed. However, do not forget to modify it in order to avoid conflicts (entering the bad amount of deviecs for example).
- **vpim_driver_setup.tar.xz** use the installation script contained in this compressed file to add the virtual PIM devices to the plugdev group in the virtual machine. You thus need to copy this in your virtual machine disk before hand.

  ## HOW TO RUN THE SYSTEM

  We use Firecracker as our virtual machine. You first need to build a rootfs. you can do so by following [this](https://happybear.medium.com/building-ubuntu-20-04-root-filesystem-for-firecracker-e3f4267e58cc) tutorial step by step.
  NB : use at least 8GB to create the virtual disk.
  Then mount the disk on your system.
  ```
  sudo mount <my_disk> /mnt/<my_folder>
  ```
  Then copy the ```apps``` folder, the ```vpim_driver_setup.tar.xz``` and the upmem SDK ```upmem_2021.3.0_amd64.deb``` file to the virtual machine disk and install all the dependencies.
  ```
  chroot /mnt/<my_folder>
  ```
  ```
  apt install libffi-dev libgcc-8-dev libnuma1 libstdc++-8-dev pkg-config python3 python3-pygments python3-serial  libedit-dev
  ```
  Once this is done, Compile the manager using the ```make``` command in the ```vpim_manager``` folder and run it with the ```vpim_manager/bin/app``` command.

  To start a virtual machine, run the nsfc-set-socket.sh in one terminal and the run-firecracker.sh to another one.
  To change the rootfs used, please change the rootfs path in the run-firecracker.sh file.
  Also modify this file if you need to change some parameters in the system like the number of vPIM devices to allocate.

  ## RUN APPLICATIONS IN THE VIRTUAL MACHINE
  First install the upmem SDK and use the script in the vpim_driver_setup.tar.xz file.
  Reboot the system to apply the changes.
  Use the ```dpu-diag``` command to check if the ranks are visible and available.
  And then go to the vPrIM folder to run the scripts : run_weak.py, run_strong_rank.py, run_strong_full.py.
