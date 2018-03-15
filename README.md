s2fs - Super Simple filesystem on Linux
====

Overview

## Description

## Usage
dd if=/dev/zero of=img bs=4096 count=100  
make  
insmod sifs.ko  
mount -o loop -t sifs img mnt  

rmmod sifs.ko

## Layout

============================================  
|  Super  | inode | file info | file data  |  
|  block  | table |   block   |    block   |  
============================================
