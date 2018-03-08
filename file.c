#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "sifs.h"


ssize_t sifs_file_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
{
	struct sifs_inode *inode = SIFS_INODE(fp->f_path.dentry->d_inode);
	struct buffer_head *bh;
	char *buffer;
	int nbytes;

	if (*ppos >= inode->file_size)
		return 0;

	printk("data block %lld\n", inode->data_block_number);
	bh = sb_bread(fp->f_path.dentry->d_inode->i_sb, inode->data_block_number);
	if (!bh) {
		printk("Unable to read the block\n");
		return 0;
	}

	buffer = (char *)bh->b_data;
	if (buffer != NULL)
		printk("content size: %ld:%lld\n", strlen(buffer), inode->file_size);
	else
		printk("buffer is NULL\n");
	nbytes = min((size_t)inode->file_size, len);

	if (copy_to_user(buf, buffer, nbytes)) {
		brelse(bh);
		printk("Unable to copy file contents to userspace\n");
		return -EFAULT;
	}

	brelse(bh);

	*ppos += nbytes;

	return nbytes;
}

const struct file_operations sifs_file_ops = {
	.read = sifs_file_read,
};
