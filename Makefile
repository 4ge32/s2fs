# Makefile
#
#

KERN_DIR  = /lib/modules/$(shell uname -r)/build
BUILD_DIR = $(shell pwd)

obj-m := sifs.o
sifs-y := super.o dir.o inode.o file.o

PROG = mkfs.sifs.c

all:
	make -C $(KERN_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=0 modules

mkfs.sifs: $(PROG)
	$(CC) $(CFLAGS) -o $@ $(PROG)
clean:
	rm -rf *.o *.ko *.mod.c *.symvers *.order *.tmp_versions mkfs.sifs
