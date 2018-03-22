# Makefile
#
#

KERN_DIR  = /lib/modules/$(shell uname -r)/build
BUILD_DIR = $(shell pwd)

obj-m := s2fs.o
s2fs-y := super.o dir.o inode.o file.o

PROG = mkfs.s2fs.c

all:
	make -C $(KERN_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=0 modules

mkfs.s2fs: $(PROG)
	$(CC) $(CFLAGS) -DMKFS -o $@ $(PROG)
clean:
	rm -rf *.o *.ko *.mod.c *.symvers *.order *.tmp_vers2ons mkfs.s2fs
