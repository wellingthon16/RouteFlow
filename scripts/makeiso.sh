#!/bin/sh
apt-get -y autoremove
apt-get -y clean
umount /var/lib/lxc
remastersys backup
rm -f /home/projectw/screenlog*
