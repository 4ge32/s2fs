#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "sifs.h"

static DEFINE_MUTEX(sifs_dir_child_update_lock);
static DEFINE_MUTEX(sifs_inodes_lock);

struct inode *sifs_iget(struct super_block *sb, int ino)
{
	struct inode *inode;
	struct sifs_inode *si_inode;

	//inode = iget_locked(sb, ino);

	si_inode = sifs_get_inode(sb, ino);

	inode = new_inode(sb);
	inode->i_ino = ino;
	inode->i_sb = sb;
	inode->i_op = &sifs_inode_ops;
	inode->i_size = si_inode->file_size;

	if (S_ISDIR(si_inode->mode))
		inode->i_fop = &sifs_dir_ops;
	else if (S_ISREG(si_inode->mode))
		inode->i_fop = &sifs_file_ops;
	else
		printk("Unknow inode type\n");

	inode->i_atime = inode->i_mtime = inode->i_ctime =
		current_time(inode);
	inode->i_private = si_inode;

	//unlock_new_inode(inode);

	return inode;
}

struct sifs_inode *sifs_get_inode(struct super_block *sb, uint64_t inode_no)
{
	int ino;
	struct sifs_sb *si_sb = sb->s_fs_info;
	struct buffer_head *bh;
	struct sifs_inode *si_inode;
	struct sifs_inode *inode_buf = NULL;

	bh = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER);
	si_inode = (struct sifs_inode *)bh->b_data;


	for (ino = 1; ino <= si_sb->inodes; ino++) {
	printk("%ld\n", si_inode->inode_no);
		if (si_inode->inode_no == inode_no) {
			inode_buf = kmem_cache_alloc(sifs_inode_cachep, GFP_KERNEL);
			memcpy(inode_buf, si_inode, sizeof(*inode_buf));
			return inode_buf;
		}
		si_inode++;
	}
	brelse(bh);
	return NULL;
}

static int sifs_sb_get_objs_count(struct super_block *sb, uint64_t *out)
{
	struct sifs_sb *si_sb = SIFS_SUPER(sb);

	if (mutex_lock_interruptible(&sifs_inodes_lock)) {
		return -EINTR;
	}

	*out = si_sb->inodes;
	mutex_unlock(&sifs_inodes_lock);

	return 0;
}


static void sifs_sb_sync(struct super_block *sb) {
	struct buffer_head *bh;
	struct sifs_sb *si_sb = SIFS_SUPER(sb);

	bh = sb_bread(sb, SIFS_SUPER_BLOCK_NUMBER);

	bh->b_data = (char *)si_sb;
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);
}

static void sifs_inode_add(struct super_block *sb, struct sifs_inode *inode)
{
	struct sifs_sb *si_sb = SIFS_SUPER(sb);
	struct buffer_head *bh;
	struct sifs_inode *si_inode;

	if (mutex_lock_interruptible(&sifs_inodes_lock)) {
		return;
	}

	bh = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER);
	si_inode = (struct sifs_inode *)bh->b_data;
	si_inode += si_sb->inodes;
	memcpy(si_inode, inode, sizeof(struct sifs_inode));
	si_sb->inodes++;
	mark_buffer_dirty(bh);
	sifs_sb_sync(sb);

	mutex_unlock(&sifs_inodes_lock);
}

static int sifs_inode_save(struct super_block *sb, struct sifs_inode *si_inode)
{
	return 0;
}

static int sifs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	struct super_block *sb;
	struct inode *inode;
	struct sifs_inode *si_inode;
	struct sifs_inode *parent_dir_inode;
	struct sifs_dir_record *new_record;
	struct buffer_head *bh;
	uint64_t count = 0;
	int ret;

	return 0;

	if (mutex_lock_interruptible(&sifs_dir_child_update_lock))
		return -EINTR;

	sb = dir->i_sb;

	ret = sifs_sb_get_objs_count(sb, &count);

	inode = new_inode(sb);
	inode->i_sb = sb;
	inode->i_op = &sifs_inode_ops;
	inode->i_atime = current_time(inode);
	inode->i_ino = (count + SIFS_ROOTDIR_INODE_NUMBER);

	si_inode = kmem_cache_alloc(sifs_inode_cachep, GFP_KERNEL);
	si_inode->inode_no = inode->i_ino;
	si_inode->mode = mode;
	inode->i_private = si_inode;

	if (S_ISDIR(mode)) {
	} else if (S_ISREG(mode)) {
		si_inode->file_size = 0;
		inode->i_fop = &sifs_file_ops;
	} else
		return -EINVAL;

	sifs_inode_add(sb, si_inode);

	parent_dir_inode = SIFS_INODE(dir);
	bh = sb_bread(sb, SIFS_ROOTDIR_DATABLOCK_NUMBER);

	new_record = (struct sifs_dir_record *)bh->b_data;
	new_record += parent_dir_inode->children_count;
	new_record->inode_no = si_inode->inode_no;
	strcpy(new_record->filename, dentry->d_name.name);

	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	brelse(bh);

	parent_dir_inode->children_count++;
	ret = sifs_inode_save(sb, parent_dir_inode);

	mutex_unlock(&sifs_dir_child_update_lock);

	inode_init_owner(inode, dir, mode);
	d_add(dentry, inode);

	return 0;
}

static struct dentry *sifs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
				  unsigned int flags)
{
	struct sifs_inode *parent = SIFS_INODE(parent_inode);
	struct super_block *sb = parent_inode->i_sb;
	struct buffer_head *bh;
	struct sifs_dir_record *record;
	int i;
	printk("LOOKUP\n");

	bh = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER + 1);
	record = (struct sifs_dir_record *)bh->b_data;
	printk("%s\n", record->filename);

	for (i = 0; i < parent->children_count; i++) {
		printk("rec: %s, child: %lld", record->filename, parent->children_count);
		if (!strcmp(record->filename, child_dentry->d_name.name)) {
			struct inode *inode = sifs_iget(sb, record->inode_no); inode_init_owner(inode, parent_inode, SIFS_INODE(inode)->mode); d_add(child_dentry, inode);
			printk("FOUND\n");
			printk("LOOKUP:%lld", SIFS_INODE(inode)->file_size);
			return NULL;
		}
		record++;
	}

	printk("NOT FOUND\n");
	return NULL;
}

const struct inode_operations sifs_inode_ops = {
	.create = sifs_create,
	.lookup = sifs_lookup,
};
