#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "sifs.h"


static ssize_t sifs_file_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
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

static ssize_t sifs_file_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct sifs_inode *si_inode;
	struct inode *inode;
	struct buffer_head *bh;
	struct super_block *sb;
	struct sifs_sb *si_sb;
	char *buffer;

	sb = fp->f_path.dentry->d_inode->i_sb;
	si_sb = SIFS_SUPER(sb);
	inode = fp->f_path.dentry->d_inode;
	si_inode = SIFS_INODE(inode);

	bh = sb_bread(sb, si_inode->data_block_number);
	buffer = (char *)bh->b_data;

	buffer += si_inode->file_size;

	if (copy_from_user(buffer, buf, len)) {
		brelse(bh);
		printk("Unable to copy file contents from userspace\n");
		return -EFAULT;
	}

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	si_inode->file_size += len;

	return len;
}

const struct file_operations sifs_file_ops = {
	.read  = sifs_file_read,
	.write = sifs_file_write,
};
