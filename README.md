# PRESENTATION OF THE FOLDER CONTENT

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