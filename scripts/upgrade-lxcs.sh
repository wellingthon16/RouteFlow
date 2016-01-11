#!/bin/sh
for i in rfvm1 b1 b2 ; do export R=/var/lib/lxc/$i/rootfs ; cp /etc/resolv.conf $R/etc/resolv.conf ; for j in "apt-get clean" "apt-get update" "apt-get -y install" "apt-get -y upgrade" "apt-get clean" "apt-get autoremove" ; do chroot $R $j ; done ; done
