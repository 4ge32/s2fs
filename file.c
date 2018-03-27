#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/quotaops.h>

#include "s2fs.h"

static DEFINE_MUTEX(s2fs_file_write_lock);

static loff_t do_s2fs_file_llseek(struct file *fp, loff_t offset, int whence,
		loff_t maxsize, loff_t eof)
{
	switch (whence) {
	case SEEK_END:
		offset += eof;
		break;
	case SEEK_CUR:
		if (offset == 0)
			return fp->f_pos;
		offset = vfs_setpos(fp, fp->f_pos + offset, maxsize);
		return offset;
	}

	return vfs_setpos(fp, offset, maxsize);
}

loff_t s2fs_file_llseek(struct file *fp, loff_t offset, int whence)
{
	struct inode *inode = fp->f_mapping->host;

	printk("LLSEEK - FILE");
	if (inode == NULL)
		printk("LLSEEK - FILE: INODE NULL");
	else {
		printk("LLSEEK - FILE: INODE NOT NULL");
		printk("LLSEEK - FILE: %ld", inode->i_ino);
	}

	return do_s2fs_file_llseek(fp, offset, whence,
				   inode->i_sb->s_maxbytes,
				   inode->i_size);
}

static int s2fs_file_open(struct inode *inode, struct file *fp)
{
	struct s2fs_inode *s2_inode = S2FS_INODE(inode);

	s2_inode->ref_count++;

	printk("OPEN - FILE: refcount %d %s\n", s2_inode->ref_count, s2_inode->rec->filename);

	if (fp->f_mode & FMODE_WRITE) {
		printk("OPEN - FILE: WRITE MODE");
	} else if (fp->f_mode & FMODE_READ) {
		printk("OPEN - FILE: READ MODE");
	} else
		printk("OPEN - FILE: OTHER MODE");

	return 0;
}

static int s2fs_file_release(struct inode *inode, struct file *fp)
{
	struct s2fs_inode *s2_inode = S2FS_INODE(inode);

	s2_inode->ref_count--;

	return 0;
}

static ssize_t s2fs_file_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
{
	struct s2fs_inode *inode = S2FS_INODE(fp->f_path.dentry->d_inode);
	struct buffer_head *bh;
	char *buffer;
	int nbytes;

	if (*ppos >= inode->file_size)
		return 0;

	printk("data block %d\n", inode->data_block_number);
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

	printk("WRITE - POS - FILE: %lld", fp->f_pos);

	sb = fp->f_path.dentry->d_inode->i_sb;
	s2_sb = S2FS_SUPER(sb);
	inode = fp->f_path.dentry->d_inode;
	s2_inode = S2FS_INODE(inode);

	bh = sb_bread(sb, s2_inode->data_block_number);
	buffer = (char *)bh->b_data;

	if (mutex_lock_interruptible(&s2fs_file_write_lock))
		return -EINTR;

	//buffer += *ppos;

	if (copy_from_user(buffer, buf, len)) {
		brelse(bh);
		printk("Unable to copy file contents from userspace\n");
		return -EFAULT;
	}

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	s2_inode->file_size = strlen(buf) - 1;
	inode->i_size = strlen(buf) - 1;

	mutex_unlock(&s2fs_file_write_lock);

	s2fs_inode_save(sb, s2_inode);

	return len;
}

static int s2fs_file_fsync(struct file *fp, loff_t start, loff_t end, int datasync)
{
	return 0;
}

const struct file_operations s2fs_file_ops = {
	.llseek  = s2fs_file_llseek,
	.open    = s2fs_file_open,
	.release = s2fs_file_release,
	.read    = s2fs_file_read,
	.write   = s2fs_file_write,
	.fsync   = s2fs_file_fsync,
};
