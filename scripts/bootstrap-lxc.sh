#!/bin/sh
cd /
start lxc 
modprobe -r bridge
modprobe openvswitch
modprobe zram
umount /var/lib/lxc
echo 2048M > /sys/block/zram0/disksize
mke2fs -q -m 0 -b 4096 -O sparse_super -L zram /dev/zram0
mount -o relatime,noexec,nosuid /dev/zram0 /var/lib/lxc
tar zxvf /home/projectw/clean-rfvm1-b1-b2.tgz
