#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/quotaops.h>
#include <linux/backing-dev.h>
#include <linux/uio.h>
#include <linux/sched/signal.h>

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
	struct s2fs_inode_info *s2_inode = S2FS_INODE(inode);

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
	struct s2fs_inode_info *s2_inode = S2FS_INODE(inode);

	s2_inode->ref_count--;

	return 0;
}

static ssize_t s2fs_file_read(struct file *fp, char __user *buf, size_t len, loff_t *ppos)
{
	struct s2fs_inode_info *inode = S2FS_INODE(fp->f_path.dentry->d_inode);
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

//static ssize_t s2fs_file_write(struct file *fp, const char __user *buf, size_t len, loff_t *ppos)
//{
//	struct s2fs_inode_info *s2_inode;
//	struct inode *inode;
//	struct buffer_head *bh;
//	struct super_block *sbi;
//	struct s2fs_sbi_info *s2_sbi;
//	char *buffer;
//
//	printk("WRITE - POS - FILE: %lld", fp->f_pos);
//
//	sbi = fp->f_path.dentry->d_inode->i_sb;
//	s2_sbi = S2FS_SUPER(sbi);
//	inode = fp->f_path.dentry->d_inode;
//	s2_inode = S2FS_INODE(inode);
//
//	bh = sb_bread(sbi, s2_inode->data_block_number);
//	buffer = (char *)bh->b_data;
//
//	if (mutex_lock_interruptible(&s2fs_file_write_lock))
//		return -EINTR;
//
//	//buffer += *ppos;
//
//	if (copy_from_user(buffer, buf, len)) {
//		brelse(bh);
//		printk("Unable to copy file contents from userspace\n");
//		return -EFAULT;
//	}
//
//	mark_buffer_dirty(bh);
//	sync_dirty_buffer(bh);
//	brelse(bh);
//
//	s2_inode->file_size = strlen(buf) - 1;
//	inode->i_size = strlen(buf) - 1;
//
//	mutex_unlock(&s2fs_file_write_lock);
//
//	s2fs_inode_info_save(sbi, s2_inode);
//
//	return len;
//}


ssize_t s2fs_perform_write(struct file *file,
	        	   struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;
	static int count = 0;

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */
		void *fsdata;

		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));

again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}

		printk("%d:HERE?", count++);
		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);
		printk("%d:HERE?", count++);
		if (unlikely(status < 0))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		printk("%d:HERE?", count++);
		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
		printk("%d:HERE?", count++);
		flush_dcache_page(page);
		printk("%d:HERE?", count++);

		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		printk("%d:HERE?", count++);
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

		balance_dirty_pages_ratelimited(mapping);
	} while (iov_iter_count(i));

	return written ? written : status;
}

ssize_t __s2fs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct address_space * mapping = file->f_mapping;
	struct inode 	*inode = mapping->host;
	ssize_t		written = 0;
	ssize_t		err;

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = inode_to_bdi(inode);
	err = file_remove_privs(file);
	if (err)
		goto out;

	err = file_update_time(file);
	if (err)
		goto out;

	printk("WRITE_ITER: UPDATE OK!\n");

	printk("WRITE_ITER: NOT DIRECT IO!\n");
	written = s2fs_perform_write(file, from, iocb->ki_pos);
	printk("FILE WRITE: %lld", inode->i_size);
	if (likely(written > 0))
		iocb->ki_pos += written;
out:
	current->backing_dev_info = NULL;
	return written ? written : err;
}

static ssize_t s2fs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	inode_lock(inode);
	ret = generic_write_checks(iocb, from);
	if (ret > 0) {
		printk("WRITE_ITER: OK\n");
		ret = __s2fs_file_write_iter(iocb, from);
	}
	inode_unlock(inode);

	if (ret > 0)
		ret = generic_write_sync(iocb, ret);
	return ret;
}

static int s2fs_file_fsync(struct file *fp, loff_t start, loff_t end, int datasync)
{
	return 0;
}

const struct file_operations s2fs_file_ops = {
	.llseek  = s2fs_file_llseek,
	.open    = s2fs_file_open,
	.release = s2fs_file_release,
	//.read    = s2fs_file_read,
	//.write   = s2fs_file_write,
	.read_iter = generic_file_read_iter,
	.write_iter   = s2fs_file_write_iter,
	.fsync   = s2fs_file_fsync,
};
