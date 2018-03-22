#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

static DEFINE_MUTEX(s2fs_sb_lock);
static DEFINE_MUTEX(s2fs_dir_child_update_lock);
static DEFINE_MUTEX(s2fs_inodes_lock);

struct inode *s2fs_iget(struct super_block *sb, int ino)
{
	struct inode *inode;
	struct s2fs_inode *s2_inode;

	//inode = iget_locked(sb, ino);

	s2_inode = s2fs_get_inode(sb, ino);

	inode = new_inode(sb);
	inode->i_ino = ino;
	inode->i_sb = sb;
	inode->i_op = &s2fs_inode_ops;
	inode->i_size = s2_inode->file_size;

	if (S_ISDIR(s2_inode->mode))
		inode->i_fop = &s2fs_dir_ops;
	else if (S_ISREG(s2_inode->mode))
		inode->i_fop = &s2fs_file_ops;
	else
		printk("Unknow inode type\n");

	inode->i_atime = inode->i_mtime = inode->i_ctime =
		current_time(inode);
	inode->i_private = s2_inode;

	//unlock_new_inode(inode);

	return inode;
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
		printk("INODE_RECORD: %lld, %lld\n", s2_inode->inode_no, record->inode_no);
		if (s2_inode->inode_no == record->inode_no) {
			printk("GET RECORD!\n");
			s2_inode->rec = record;
			goto FOUND;
		}
		record++;
		printk("KERN ken  ");
	}
	brelse(bh);
	return 1;
FOUND:
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
	printk("%lld\n", s2_inode->inode_no);
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

int s2fs_inode_save(struct super_block *sb, struct s2fs_inode *s2_inode)
{
	struct s2fs_inode *s2_inode_itr;
	struct buffer_head *bh;

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);

	if (mutex_lock_interruptible(&s2fs_sb_lock)) {
		return -EINTR;
	}

	s2_inode_itr = s2fs_inode_search(sb, (struct s2fs_inode *)bh->b_data, s2_inode);

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

static int s2fs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	struct super_block *sb;
	struct inode *inode;
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

	ret = s2fs_sb_get_objs_count(sb, &count);

	inode = new_inode(sb);
	inode->i_sb = sb;
	inode->i_op = &s2fs_inode_ops;
	inode->i_atime = current_time(inode);
	inode->i_ino = (count + S2FS_ROOTDIR_INODE_NUMBER);

	s2_inode = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
	s2_inode->inode_no = inode->i_ino;
	s2_inode->mode = mode;
	s2_inode->valid = true;
	inode->i_private = s2_inode;


	if (S_ISDIR(mode)) {
		printk(KERN_INFO "New directory creation request\n");
		s2_inode->children_count = 0;
		inode->i_fop = &s2fs_dir_ops;
	} else if (S_ISREG(mode)) {
		printk(KERN_INFO "New file creation request\n");
		s2_inode->file_size = 0;
		s2_inode->data_block_number = s2_inode->inode_no +
					S2FS_RECORD_BLOCK_NUMBER - 1;
		inode->i_fop = &s2fs_file_ops;
	} else
		return -EINVAL;


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
	int i;
	printk("LOOKUP\n");

	bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER + 1);
	record = (struct s2fs_dir_record *)bh->b_data;
	printk("%s\n", record->filename);

	for (i = 0; i < parent->children_count; i++) {
		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			struct inode *inode = s2fs_iget(sb, record->inode_no);
			inode_init_owner(inode, parent_inode, S2FS_INODE(inode)->mode);
			d_add(child_dentry, inode);
			printk("FOUND\n");
			printk("LOOKUP:%lld\n", S2FS_INODE(inode)->file_size);
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

	inode->i_ctime = dir->i_ctime = dir->i_mtime = current_time(inode);
	printk("NOTICE: Unlink old %d and ino %ld\n", s2_inode->valid, dentry->d_inode->i_ino);
	s2_inode->valid = false;
	s2fs_inode_save(sb, s2_inode);
	printk("NOTICE: Unlink old %d\n", s2_inode->valid);
	drop_nlink(inode);
	dput(dentry);
	return 0;
}

const struct inode_operations s2fs_inode_ops = {
	.create = s2fs_create,
	.lookup = s2fs_lookup,
	.mkdir  = s2fs_mkdir,
	.unlink = s2fs_unlink,
};
