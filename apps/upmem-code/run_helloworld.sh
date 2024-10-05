rm *.log
gcc --std=c99 helloworld_host.c -o helloworld_host `dpu-pkg-config --cflags --libs dpu`
UPMEM_VERBOSE=v UPMEM_PROFILE="regionMode=safe" ./helloworld_host
