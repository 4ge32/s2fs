# Makefile
#
#

KERN_DIR  = /lib/modules/$(shell uname -r)/build
BUILD_DIR = $(shell pwd)

obj-m := super.o

PROG = super.c

all:
	make -C $(KERN_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=0 modules
clean:
	rm -rf *.o *.ko *.mod.c *.symvers *.order *.tmp_versions
