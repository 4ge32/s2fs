#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "s2fs.h"


static ssize_t s2fs_file_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
{
	struct s2fs_inode *inode = S2FS_INODE(fp->f_path.dentry->d_inode);
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

static ssize_t s2fs_file_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct s2fs_inode *s2_inode;
	struct inode *inode;
	struct buffer_head *bh;
	struct super_block *sb;
	struct s2fs_sb *s2_sb;
	char *buffer;

	sb = fp->f_path.dentry->d_inode->i_sb;
	s2_sb = S2FS_SUPER(sb);
	inode = fp->f_path.dentry->d_inode;
	s2_inode = S2FS_INODE(inode);

	bh = sb_bread(sb, s2_inode->data_block_number);
	buffer = (char *)bh->b_data;

	buffer += s2_inode->file_size;

	if (copy_from_user(buffer, buf, len)) {
		brelse(bh);
		printk("Unable to copy file contents from userspace\n");
		return -EFAULT;
	}

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	s2_inode->file_size += len;
	s2fs_inode_save(sb, s2_inode);

	return len;
}

const struct file_operations s2fs_file_ops = {
	.read  = s2fs_file_read,
	.write = s2fs_file_write,
};
