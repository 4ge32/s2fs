s2fs - Super Simple filesystem on Linux
====

## Description

## Usage

```
dd if=/dev/zero of=img bs=4096 count=100  
make  
insmod s2fs.ko  
mount -o loop -t s2fs img mnt  

rmmod s2fs.ko
```

## Layout 

```
============================================= 　
|  Super  | inode | file info | file data   |  　
|  block  | table |   block   |    block    |
=============================================
```

## Next Work
1. Add test script. 
2. Clean the entire code. 
