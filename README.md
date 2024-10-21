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

  ## BEFORE RUNNING OUR SYSTEM

  We use Firecracker as our virtual machine. You first need to build a rootfs. you can do so by following [this](https://happybear.medium.com/building-ubuntu-20-04-root-filesystem-for-firecracker-e3f4267e58cc) tutorial step by step.
  NB : use at least 8GB to create the virtual disk.
  Then mount the disk on your system.
  ```
  sudo mount <my_disk> /mnt/<my_folder>
  ```
  Then copy the ```apps``` folder and the ```vpim_driver_setup.tar.xz```  file to the virtual machine disk.
  ```
  sudo cp <path to the root of the project>/apps /mnt/<my_folder>/root/
  ```
  ```
  sudo cp <path to the root of the project>/code/vpim_driver_setup.tar.xz /mnt/<my_folder>/root/vpim_driver_setup.tar.xz
  ```
   Install the required dependencies in the rootfs. 
  ```
  chroot /mnt/<my_folder>
  ```
  ```
  apt install libffi-dev libgcc-8-dev libnuma1 libstdc++-8-dev pkg-config python3 python3-pygments python3-serial  libedit-dev
  ```
  The rootfs is now ready to be used.

  ## RUN THE MANAGER
  In order to run the manager, you need to go the manager folder from the root of the project :
  ```
  cd code/vpim_manager
  ```
  Compile the manager using the ```make``` command in the ```vpim_manager``` folder.

  Run the manager with the ```vpim_manager/bin/app``` command.

The manager should now present the list of the ranks available in the system. you can compare with the output of the ```dpu-diag``` command to make sure that you have all the ranks listed. 

## START THE VIRTUAL MACHINE 

  To start a virtual machine, you need to run 2 scripts. 
  The first one is in charge of building the Firecracker socket that will receive requests. From the root of the project, execute: 
  ```
  cd code
  ```
  ```
  sudo ./nsfc-set-socket.sh
  ```
  The second script to run sends requests for the resources to allocate to the virtual machine (the kernel, the rootfs, the balloon, the vpim devices etc.).
  ```
  sudo ./run-firecracker.sh
  ```
  This should be run in another terminal. 
  That second script can be modified to change the allocated resources. 
  To change the rootfs used, please change the rootfs path in the run-firecracker.sh file. This is particularly important to setup the rootfs path to the one you have created earlier. 
  You can also adjust the number of ranks to allocate on the system. by adding or removing requests in the run_firecracker.sh. These requests look like : 
  ```
 curl --unix-socket /tmp/firecracker.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
      "hold_rank":1
    }'
```
You can add them or remove them in the file. 
NOTE: there are two other files with these names with the suffix "2" at the end of the filename, These are in case you need to run a second virtual machine. You just follow the same pattern. by running : 
  ```
  sudo ./nsfc-set-socket2.sh
  ```
and 
  ```
  sudo ./run-firecracker2.sh
  ```
The virtual machine is now up and running. 

  ## SETUP THE VIRTUAL MACHINE ENVIRONMENT
  NOTE: When you run applications on UPMEM hardware (VM or not) you can encounter a "Wavegen error" that is an internal error of these hardwares. If that occurs, please do a power cycle of your server with the command ```sudo ipmitool power cycle```. if the error occured in the VM, shutdown the VM first.

  If it is the first time that you run that virtual machine, you will first need to install the UPMEM SDK as in the native system. Remember the apps folder that you copied in the rootfs. go into that folder.
```
  cd apps
```
Install the package named ```upmem_2021.3.0_amd64.deb```
```
dpkg -i upmem_2021.3.0_amd64.deb
```
Once it is done, extract the content of ```vpim_driver_setup.tar.xz file```. from the apps folder :
```
  cd ..
```
```
tar -xvf vpim_driver_setup.tar.xz
```
Go to the extracted folder
```
cd vpim_driver_setup/
```
Give the execute permission to the script inside :
```
chmod +x ./vpim_userspace_setup.sh
```
Execute that script in order to add UPMEM devices to the ```plugdev``` group
```
sudo ./vpim_userspace_setup.sh
```
Now reboot the virtual machine to apply the changes by first shutting it down with the ```reboot``` command and follow the process described above to run a virtul machine with the two scripts. 
```
reboot
```
Use the ```dpu-diag``` command in the virtual machine to check if the ranks are visible and available.
```
dpu-diag
```
The dpu-diag output should give the rank descriptions as it does in the native environment. 
## RUN PROGRAMS IN THE VIRTUAL MACHINE
You can run the vPrIM example with one of these scripts : ```run_vprim.py```, ```run_strong_rank.py```, ```run_strong_full.py```.
First go in the correct folder : 
```
cd apps/vPrIM
```
You can do for example : 
```
python3 run_strong_rank.py VA
```
This will run the VA program from the PrIM benchmark. The application list is available [there](https://github.com/CMU-SAFARI/prim-benchmarks.git)
 
