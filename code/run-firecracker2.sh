#/bin/bash
# set the kernel
arch=`uname -m`
#kernel_path=$(pwd)"/vmlinux.bin"
#kernel_path="./guest-5.10.98/vmlinux"
kernel_path="../../develop2/pim/code/guest-5.10.98/vmlinux"
#kernel_path="../../platform/pim/code/guest-5.10.98/vmlinux"

if [ ${arch} = "x86_64" ]; then
curl --unix-socket /tmp/firecracker2.socket -i \
      -X PUT 'http://localhost/boot-source'   \
      -H 'Accept: application/json'           \
      -H 'Content-Type: application/json'     \
      -d "{
            \"kernel_image_path\": \"${kernel_path}\",
            \"boot_args\": \"console=ttyS0 reboot=k panic=1 pci=off\"
       }"
elif [ ${arch} = "aarch64" ]; then
curl --unix-socket /tmp/firecracker2.socket -i \
      -X PUT 'http://localhost/boot-source'   \
      -H 'Accept: application/json'           \
      -H 'Content-Type: application/json'     \
      -d "{
            \"kernel_image_path\": \"${kernel_path}\",
            \"boot_args\": \"keep_bootcon console=ttyS0 reboot=k panic=1 pci=off\"
       }"
else
    echo "Cannot run firecracker on $arch architecture!"
    exit 1
fi

#set the rootfs
#rootfs_path=$(pwd)"/bionic.rootfs.ext4"
#rootfs_path=$(pwd)"/../../develop2/pim/code/focal.rootfs.ext4"
rootfs_path="/home/pim/Documents/rootfs/vm2_ubuntu-20_04.ext4"
#rootfs_path=$(pwd)"/focal.rootfs.ext4"
#rootfs_path="/boot/initrd.img-5.10.98"
curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/drives/rootfs' \
  -H 'Accept: application/json'           \
  -H 'Content-Type: application/json'     \
  -d "{
        \"drive_id\": \"rootfs\",
        \"path_on_host\": \"${rootfs_path}\",
        \"is_root_device\": true,
        \"is_read_only\": false
   }"

# config vcpus memory and other stuff
 curl --unix-socket /tmp/firecracker2.socket -i  \
  -X PUT 'http://localhost/machine-config' \
  -H 'Accept: application/json'            \
  -H 'Content-Type: application/json'      \
  -d '{
      "vcpu_count": 4,
      "mem_size_mib": 10144
  }'
 


# set the balloon 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/balloon' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
      "amount_mib": 128,
      "deflate_on_oom": true,
      "stats_polling_interval_s": 10
    }'


# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

 set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

 set the vpim device 
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

# set the vpim device 
# curl --unix-socket /tmp/firecracker2.socket -i \
#  -X PUT 'http://localhost/vpim' \
#  -H 'Accept: application/json' \
#  -H 'Content-Type: application/json' \
#  -d '{
#    }'


   
# set the netwørk device 
#curl --unix-socket /tmp/firecracker2.socket -i \
#  -X PUT 'http://localhost/network-interfaces/eth0' \
#  -H 'Accept: application/json' \
#  -H 'Content-Type: application/json' \
#  -d '{
#      "iface_id": "eth0",
#      "host_dev_name": "tap0"
#    }'
#start the guest machine
 curl --unix-socket /tmp/firecracker2.socket -i \
  -X PUT 'http://localhost/actions'       \
  -H  'Accept: application/json'          \
  -H  'Content-Type: application/json'    \
  -d '{
      "action_type": "InstanceStart"
   }'
