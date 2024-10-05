#/bin/bash
clear
rm -f /tmp/firecracker2.socket
./firecracker/build/cargo_target/x86_64-unknown-linux-musl/debug/firecracker --api-sock /tmp/firecracker2.socket --no-seccomp
