#include <linux/module.h>
#include <linux/fs.h>
#include "sifs.h"

static int sifs_readdir(struct file *fp, struct dir_context *ctx) {
	return 0;
}

const struct file_operations sifs_dir_ops = {
	.llseek         = generic_file_llseek,
	.read           = generic_read_dir,
	.iterate_shared = sifs_readdir,
};

const struct inode_operations sifs_inode_ops = {
};
