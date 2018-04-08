#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

static DEFINE_MUTEX(s2fs_sb_info_lock);
static DEFINE_MUTEX(s2fs_dir_child_update_lock);
static DEFINE_MUTEX(s2fs_inode_infos_lock);

static struct s2fs_inode_info *s2fs_inode_info_search(struct super_block *,
						struct s2fs_inode_info *,
						struct s2fs_inode_info *);

static int s2fs_set_inode(struct super_block *sb, struct inode *inode, struct s2fs_inode_info *s2_inode)
{
	int ret;

	inode->i_sb = sb;
	//inode->i_size = 4096;
	inode->i_op = &s2fs_inode_ops;
	inode->i_ino = s2_inode->inode_no;
	inode->i_mode = s2_inode->mode;
	inode->i_size = s2_inode->file_size;
	inode->i_mapping->a_ops = &s2fs_aops;
	inode->i_atime = inode->i_mtime = inode->i_ctime =
		current_time(inode);
	if (S_ISDIR(inode->i_mode)) {
		inode->i_fop = &s2fs_dir_ops;
		ret = DIRECTORY;
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_fop = &s2fs_file_ops;
		ret = REGFILE;
	} else
		return -EINVAL;
	inode_init_owner(inode, NULL, inode->i_mode);
	inode->i_private = s2_inode;

	return ret;
}

int s2fs_inode_info_save(struct super_block *sbi, struct s2fs_inode_info *s2_inode)
{
	struct s2fs_inode_info *s2_inode_itr;
	struct buffer_head *bh;

	bh = sb_bread(sbi, S2FS_INODE_STORE_BLOCK_NUMBER);

	s2_inode_itr = s2fs_inode_info_search(sbi, (struct s2fs_inode_info *)bh->b_data, s2_inode);

	if (mutex_lock_interruptible(&s2fs_sb_info_lock)) {
		return -EINTR;
	}

	if (s2_inode_itr) {
		memcpy(s2_inode_itr, s2_inode, sizeof(*s2_inode_itr));
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
	} else {
		brelse(bh);
		mutex_unlock(&s2fs_sb_info_lock);
		return -EIO;
	}

	brelse(bh);

	mutex_unlock(&s2fs_sb_info_lock);

	return 0;

}

int s2fs_get_inode_record(struct super_block *sbi, struct s2fs_inode_info *s2_inode)
{
	struct buffer_head *bh;
	struct s2fs_dir_record *record;
	struct s2fs_sb_info *s2_sbi = S2FS_SUPER(sbi);
	int ino;

	bh= sb_bread(sbi, S2FS_RECORD_BLOCK_NUMBER);
	record = (struct s2fs_dir_record *)bh->b_data;

	for (ino = 1; ino <= s2_sbi->s_ninodes; ino++) {
		printk("INODE_RECORD: %d, %d\n", s2_inode->inode_no, record->inode_no);
		if (s2_inode->inode_no == record->inode_no) {
			printk("GET RECORD!\n");
			s2_inode->rec = record;
			goto FOUND;
		}
		record++;
	}

	brelse(bh);

	return 1;
FOUND:
	brelse(bh);
	return 0;
}

struct s2fs_inode_info *s2fs_get_inode(struct super_block *sbi, uint64_t inode_no)
{
	int ino;
	struct s2fs_sb_info *s2_sbi = sbi->s_fs_info;
	struct buffer_head *bh;
	struct s2fs_inode_info *s2_inode;
	struct s2fs_inode_info *inode_buf = NULL;

	bh = sb_bread(sbi, S2FS_INODE_STORE_BLOCK_NUMBER);
	s2_inode = (struct s2fs_inode_info *)bh->b_data;

	for (ino = 1; ino <= s2_sbi->s_ninodes; ino++) {
		printk("%d\n", s2_inode->inode_no);
		if (!s2_inode->valid)
			continue;
		if (s2_inode->inode_no == inode_no) {
			printk("sehen sich mich!\n");
			s2fs_get_inode_record(sbi, s2_inode);
			inode_buf = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
			memcpy(inode_buf, s2_inode, sizeof(*inode_buf));
			return inode_buf;
		}
		s2_inode++;
	}
	brelse(bh);

	return NULL;
}

struct inode *s2fs_iget(struct super_block *sbi, int ino)
{
	struct inode *inode;
	struct s2fs_inode_info *s2_inode;

	printk("S2FS_IGET");

	//inode = iget_locked(sbi, ino);

	s2_inode = s2fs_get_inode(sbi, ino);
	if (s2_inode == NULL)
		return NULL;
	inode = new_inode(sbi);
	s2fs_set_inode(sbi, inode, s2_inode);

	//unlock_new_inode(inode);

	return inode;
}

static int s2fs_sb_info_get_objs_count(struct super_block *sbi, uint64_t *out)
{
	struct s2fs_sb_info *s2_sbi = S2FS_SUPER(sbi);

	if (mutex_lock_interruptible(&s2fs_inode_infos_lock)) {
		return -EINTR;
	}

	*out = s2_sbi->s_ninodes;

	mutex_unlock(&s2fs_inode_infos_lock);

	return 0;
}

static void s2fs_sb_info_sync(struct super_block *sbi) {
	struct buffer_head *bh;
	struct s2fs_sb_info *s2_sbi = S2FS_SUPER(sbi);

	bh = sb_bread(sbi, S2FS_SUPER_BLOCK_NUMBER);
	bh->b_data = (char *)s2_sbi;
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

	brelse(bh);
}

static void s2fs_inode_info_add(struct super_block *sbi, struct s2fs_inode_info *inode)
{
	struct s2fs_sb_info *s2_sbi = S2FS_SUPER(sbi);
	struct buffer_head *bh;
	struct s2fs_inode_info *s2_inode;

	if (mutex_lock_interruptible(&s2fs_inode_infos_lock)) {
		return;
	}

	bh = sb_bread(sbi, S2FS_INODE_STORE_BLOCK_NUMBER);
	s2_inode = (struct s2fs_inode_info *)bh->b_data;
	s2_inode += s2_sbi->s_ninodes;
	memcpy(s2_inode, inode, sizeof(struct s2fs_inode_info));
	s2_sbi->s_ninodes++;

	mark_buffer_dirty(bh);
	s2fs_sb_info_sync(sbi);

	mutex_unlock(&s2fs_inode_infos_lock);
}

static struct s2fs_inode_info *s2fs_inode_info_search(struct super_block *sbi,
						struct s2fs_inode_info *begin,
						struct s2fs_inode_info *target)
{
	uint64_t count = 0;

	while (begin->inode_no != target->inode_no
			&& count < S2FS_SUPER(sbi)->s_ninodes) {
		count++;
		begin++;
	}

	if (begin->inode_no == target->inode_no) {
		return begin;
	}

	return NULL;
}

static int s2fs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	struct super_block *sbi;
	struct inode *inode = NULL;
	struct s2fs_inode_info *s2_inode;
	struct s2fs_inode_info *parent_dir_inode;
	struct s2fs_dir_record *new_record;
	struct buffer_head *bh;
	uint64_t count = 0;
	int ret;

	printk("CREATE\n");
	printk("mode:%d\n", mode);

	if (mutex_lock_interruptible(&s2fs_dir_child_update_lock))
		return -EINTR;

	sbi = dir->i_sb;

	s2fs_sb_info_get_objs_count(sbi, &count);

	s2_inode = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
	s2_inode->inode_no = (count + S2FS_ROOT_INO);
	s2_inode->mode = mode;
	s2_inode->valid = true;
	s2_inode->file_size = 0;
	s2_inode->ref_count = 0;

	inode = new_inode(sbi);
	ret = s2fs_set_inode(sbi, inode, s2_inode);
	if (ret == DIRECTORY)
		printk(KERN_INFO "New directory creation request\n");
	else if (ret == REGFILE) {
		printk(KERN_INFO "New file creation request\n");
		s2_inode->data_block_number = s2_inode->inode_no +
					S2FS_RECORD_BLOCK_NUMBER - 1;
	} else {
		printk("INODE - CREATE: OTHER");
		return ret;
	}

	s2fs_inode_info_add(sbi, s2_inode);

	parent_dir_inode = S2FS_INODE(dir);
	bh = sb_bread(sbi, S2FS_RECORD_BLOCK_NUMBER);
	/* ORDER is important */
	new_record = (struct s2fs_dir_record *)bh->b_data;
	new_record += parent_dir_inode->inode_no + parent_dir_inode->children_count - 1;
	new_record->inode_no = s2_inode->inode_no;
	strcpy(new_record->filename, dentry->d_name.name);
	s2_inode->rec = new_record;

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	parent_dir_inode->children_count++;
	ret = s2fs_inode_info_save(sbi, parent_dir_inode);

	mutex_unlock(&s2fs_dir_child_update_lock);

	inode_init_owner(inode, dir, mode);
	d_add(dentry, inode);

	return 0;
}

static struct dentry *s2fs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
				  unsigned int flags)
{
	struct s2fs_inode_info *parent = S2FS_INODE(parent_inode);
	struct super_block *sbi = parent_inode->i_sb;
	struct buffer_head *bh;
	struct s2fs_dir_record *record;
	int ino;
	printk("LOOKUP\n");

	bh = sb_bread(sbi, S2FS_RECORD_BLOCK_NUMBER);
	record = (struct s2fs_dir_record *)bh->b_data;
	printk("%s\n", record->filename);

	for (ino = 0; ino < parent->children_count; ino++) {

		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			struct inode *inode = s2fs_iget(sbi, record->inode_no);
			if (inode == NULL)
				goto out;

			inode_init_owner(inode, parent_inode, S2FS_INODE(inode)->mode);
			d_add(child_dentry, inode);

			printk("FOUND\n");
			printk("LOOKUP:%d\n", S2FS_INODE(inode)->file_size);

			return NULL;
		}
		record++;
	}

out:
	printk("NOT FOUND\n");

	return NULL;
}

static int s2fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return s2fs_create(dir, dentry, mode | S_IFDIR, 0);
}

static int s2fs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct s2fs_inode_info *s2_inode = S2FS_INODE(inode);
	struct super_block *sbi = dir->i_sb;

	printk("INODE - UNLINK: %ld", inode->i_ino);

	inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
	s2_inode->valid = false;
	s2fs_inode_info_save(sbi, s2_inode);
	drop_nlink(inode);
	dput(dentry);

	printk("NOTICE: Unlink old %d\n", s2_inode->valid);
	printk("NOTICE: Unlink old %d and ino %ld\n", s2_inode->valid, dentry->d_inode->i_ino);

	return 0;
}

static int s2fs_rename(struct inode * old_dir, struct dentry * old_dentry,
			struct inode * new_dir,	struct dentry * new_dentry,
			unsigned int flags)
{
	printk("INODE - RENAME");
	return 0;
}

const struct inode_operations s2fs_inode_ops = {
	.create = s2fs_create,
	.lookup = s2fs_lookup,
	.mkdir  = s2fs_mkdir,
	//.unlink = s2fs_unlink,
	.rename = s2fs_rename,
};

static int s2fs_get_block(struct inode *inode, sector_t block,
			  struct buffer_head *bh, int create)
{
	return 1;
}

static int
s2fs_write_begin(struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, flags, pagep,
				s2fs_get_block);
	//if (ret < 0)
	//	ext2_write_failed(mapping, pos + len);
	return ret;
}

static int s2fs_write_end(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned copied,
			struct page *page, void *fsdata)
{
	int ret;

	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	//if (ret < len)
	//	ext2_write_failed(mapping, pos + len);
	return ret;
}

static int h(struct page *page, struct writeback_control *wbc)
{
	return 0;
}

const struct address_space_operations s2fs_aops = {
	.readpage               = simple_readpage,
	.writepage              = h,
	.write_begin		= simple_write_begin,
	.write_end		= simple_write_end,
};
