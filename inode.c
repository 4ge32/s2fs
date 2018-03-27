#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

static DEFINE_MUTEX(s2fs_sb_lock);
static DEFINE_MUTEX(s2fs_dir_child_update_lock);
static DEFINE_MUTEX(s2fs_inodes_lock);

static struct s2fs_inode *s2fs_inode_search(struct super_block *,
						struct s2fs_inode *,
						struct s2fs_inode *);

static int s2fs_set_inode(struct super_block *sb, struct inode *inode, struct s2fs_inode *s2_inode)
{
	int ret;

	inode->i_sb = sb;
	inode->i_op = &s2fs_inode_ops;
	inode->i_ino = s2_inode->inode_no;
	inode->i_mode = s2_inode->mode;
	inode->i_size = s2_inode->file_size;
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
	inode->i_private = s2_inode;

	return ret;
}

int s2fs_inode_save(struct super_block *sb, struct s2fs_inode *s2_inode)
{
	struct s2fs_inode *s2_inode_itr;
	struct buffer_head *bh;

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);

	s2_inode_itr = s2fs_inode_search(sb, (struct s2fs_inode *)bh->b_data, s2_inode);

	if (mutex_lock_interruptible(&s2fs_sb_lock)) {
		return -EINTR;
	}

	if (s2_inode_itr) {
		memcpy(s2_inode_itr, s2_inode, sizeof(*s2_inode_itr));
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
	} else {
		brelse(bh);
		mutex_unlock(&s2fs_sb_lock);
		return -EIO;
	}

	brelse(bh);

	mutex_unlock(&s2fs_sb_lock);

	return 0;

}

int s2fs_get_inode_record(struct super_block *sb, struct s2fs_inode *s2_inode)
{
	struct buffer_head *bh;
	struct s2fs_dir_record *record;
	struct s2fs_sb *s2_sb = S2FS_SUPER(sb);
	int ino;

	bh= sb_bread(sb, S2FS_RECORD_BLOCK_NUMBER);
	record = (struct s2fs_dir_record *)bh->b_data;

	for (ino = 1; ino <= s2_sb->inodes_count; ino++) {
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

struct s2fs_inode *s2fs_get_inode(struct super_block *sb, uint64_t inode_no)
{
	int ino;
	struct s2fs_sb *s2_sb = sb->s_fs_info;
	struct buffer_head *bh;
	struct s2fs_inode *s2_inode;
	struct s2fs_inode *inode_buf = NULL;

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);
	s2_inode = (struct s2fs_inode *)bh->b_data;

	for (ino = 1; ino <= s2_sb->inodes_count; ino++) {
		printk("%d\n", s2_inode->inode_no);
		if (s2_inode->inode_no == inode_no) {
			printk("sehen sich mich!\n");
			s2fs_get_inode_record(sb, s2_inode);
			inode_buf = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
			memcpy(inode_buf, s2_inode, sizeof(*inode_buf));
			return inode_buf;
		}
		s2_inode++;
	}
	brelse(bh);

	return NULL;
}

struct inode *s2fs_iget(struct super_block *sb, int ino)
{
	struct inode *inode;
	struct s2fs_inode *s2_inode;

	printk("S2FS_IGET");

	//inode = iget_locked(sb, ino);

	s2_inode = s2fs_get_inode(sb, ino);
	inode = new_inode(sb);
	s2fs_set_inode(sb, inode, s2_inode);

	//unlock_new_inode(inode);

	return inode;
}

static int s2fs_sb_get_objs_count(struct super_block *sb, uint64_t *out)
{
	struct s2fs_sb *s2_sb = S2FS_SUPER(sb);

	if (mutex_lock_interruptible(&s2fs_inodes_lock)) {
		return -EINTR;
	}

	*out = s2_sb->inodes_count;

	mutex_unlock(&s2fs_inodes_lock);

	return 0;
}

static void s2fs_sb_sync(struct super_block *sb) {
	struct buffer_head *bh;
	struct s2fs_sb *s2_sb = S2FS_SUPER(sb);

	bh = sb_bread(sb, S2FS_SUPER_BLOCK_NUMBER);
	bh->b_data = (char *)s2_sb;
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);

	brelse(bh);
}

static void s2fs_inode_add(struct super_block *sb, struct s2fs_inode *inode)
{
	struct s2fs_sb *s2_sb = S2FS_SUPER(sb);
	struct buffer_head *bh;
	struct s2fs_inode *s2_inode;

	if (mutex_lock_interruptible(&s2fs_inodes_lock)) {
		return;
	}

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);
	s2_inode = (struct s2fs_inode *)bh->b_data;
	s2_inode += s2_sb->inodes_count;
	memcpy(s2_inode, inode, sizeof(struct s2fs_inode));
	s2_sb->inodes_count++;

	mark_buffer_dirty(bh);
	s2fs_sb_sync(sb);

	mutex_unlock(&s2fs_inodes_lock);
}

static struct s2fs_inode *s2fs_inode_search(struct super_block *sb,
						struct s2fs_inode *begin,
						struct s2fs_inode *target)
{
	uint64_t count = 0;

	while (begin->inode_no != target->inode_no
			&& count < S2FS_SUPER(sb)->inodes_count) {
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
	struct super_block *sb;
	struct inode *inode = NULL;
	struct s2fs_inode *s2_inode;
	struct s2fs_inode *parent_dir_inode;
	struct s2fs_dir_record *new_record;
	struct buffer_head *bh;
	uint64_t count = 0;
	int ret;

	printk("CREATE\n");
	printk("mode:%d\n", mode);

	if (mutex_lock_interruptible(&s2fs_dir_child_update_lock))
		return -EINTR;

	sb = dir->i_sb;

	s2fs_sb_get_objs_count(sb, &count);

	s2_inode = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
	s2_inode->inode_no = (count + S2FS_ROOTDIR_INODE_NUMBER);
	s2_inode->mode = mode;
	s2_inode->valid = true;
	s2_inode->file_size = 0;

	inode = new_inode(sb);
	ret = s2fs_set_inode(sb, inode, s2_inode);
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

	s2fs_inode_add(sb, s2_inode);

	parent_dir_inode = S2FS_INODE(dir);
	bh = sb_bread(sb, S2FS_RECORD_BLOCK_NUMBER);
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
	ret = s2fs_inode_save(sb, parent_dir_inode);

	mutex_unlock(&s2fs_dir_child_update_lock);

	inode_init_owner(inode, dir, mode);
	d_add(dentry, inode);

	return 0;
}

static struct dentry *s2fs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
				  unsigned int flags)
{
	struct s2fs_inode *parent = S2FS_INODE(parent_inode);
	struct super_block *sb = parent_inode->i_sb;
	struct buffer_head *bh;
	struct s2fs_dir_record *record;
	int child;
	printk("LOOKUP\n");

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER + 1);
	record = (struct s2fs_dir_record *)bh->b_data;
	printk("%s\n", record->filename);

	for (child = 0; child < parent->children_count; child++) {

		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			struct inode *inode = s2fs_iget(sb, record->inode_no);

			inode_init_owner(inode, parent_inode, S2FS_INODE(inode)->mode);
			d_add(child_dentry, inode);

			printk("FOUND\n");
			printk("LOOKUP:%d\n", S2FS_INODE(inode)->file_size);

			return NULL;
		}
		record++;
	}

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
	struct s2fs_inode *s2_inode = S2FS_INODE(inode);
	struct super_block *sb = dir->i_sb;

	printk("INODE - UNLINK: %ld", inode->i_ino);

	inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
	s2_inode->valid = false;
	s2fs_inode_save(sb, s2_inode);
	drop_nlink(inode);
	dput(dentry);

	printk("NOTICE: Unlink old %d\n", s2_inode->valid);
	printk("NOTICE: Unlink old %d and ino %ldd\n", s2_inode->valid, dentry->d_inode->i_ino);

	return 0;
}

const struct inode_operations s2fs_inode_ops = {
	.create = s2fs_create,
	.lookup = s2fs_lookup,
	.mkdir  = s2fs_mkdir,
	.unlink = s2fs_unlink,
};
