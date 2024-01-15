#!/bin/bash

# Load the kernel module
#sudo insmod hc-sr04.ko

# Parse major driver number from /proc/devices output
drv_major=$(awk "/distsensor/ {print \$1}" /proc/devices)

mknod /dev/distsensor c $drv_major 0
