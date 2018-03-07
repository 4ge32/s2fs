#include <linux/fs.h>

const struct file_operations sifs_file_ops = {
	.read_iter = generic_file_read_iter,
};
