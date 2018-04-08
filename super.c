/* ISSUE 1:
 * How do I change BLOCK SIZE?
 * sb_bread will be failure when block_size set except 4096.
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>

#include "s2fs.h"

struct kmem_cache *s2fs_inode_cachep;

static void s2fs_truncate(struct inode * inode)
{
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode)))
		return;
}

static void s2fs_free_inode(struct inode *inode)
{
	return;
}

static struct s2fs_inode *s2fs_raw_inode(struct super_block *sb, ino_t ino,
					 struct buffer_head **bh)
{
	struct s2fs_inode *p;

	*bh = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);
	if (!*bh)
		return NULL;

	p = (struct s2fs_inode *)(*bh)->b_data;
	return (p += ino - 1);
}

static struct buffer_head *s2fs_update_inode(struct inode *inode)
{
	struct buffer_head *bh;
	struct s2fs_inode *raw_inode;

	raw_inode = s2fs_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	inode->i_mode = raw_inode->i_mode;
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
	raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
	raw_inode->i_nlinks = inode->i_nlink;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_time = inode->i_mtime.tv_sec;
	mark_buffer_dirty(bh);

	return bh;
}

static struct inode *s2fs_alloc_inode(struct super_block *sb)
{
	struct s2fs_inode_info *ei;

	ei = kmem_cache_alloc(s2fs_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void s2fs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(s2fs_inode_cachep, s2fs_i(inode));
}

static void s2fs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, s2fs_i_callback);
}

static int s2fs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	int err = 0;
	struct buffer_head *bh;

	bh = s2fs_update_inode(inode);
	if (!bh)
		return -EIO;
	if (wbc->sync_mode == WB_SYNC_ALL && buffer_dirty(bh)) {
		sync_dirty_buffer(bh);
		if (buffer_req(bh) && !buffer_uptodate(bh)) {
			printk("IO error syncing s2fs inode [%s:%08lx]\n",
				inode->i_sb->s_id, inode->i_ino);
			err = -EIO;
		}
	}
	brelse (bh);
	return err;
}

static void s2fs_evict_inode(struct inode *inode)
{
	truncate_inode_pages_final(&inode->i_data);
	if (!inode->i_nlink) {
		inode->i_size = 0;
		s2fs_truncate(inode);
	}
	invalidate_inode_buffers(inode);
	clear_inode(inode);
	if (!inode->i_nlink)
		s2fs_free_inode(inode);
}

static void s2fs_put_super(struct super_block *sb)
{
	struct s2fs_sb_info *sbi = S2FS_SUPER(sb);

	brelse(sbi->s_sbh);
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static const struct super_operations s2fs_sops = {
	.alloc_inode   = s2fs_alloc_inode,
	.destroy_inode = s2fs_destroy_inode,
	.write_inode   = s2fs_write_inode,
	//.evict_inode   = s2fs_evict_inode,
	.put_super     = s2fs_put_super,
};

static int init_inodecache(void)
{
	s2fs_inode_cachep = kmem_cache_create("s2fs_inode_info_cache",
					      sizeof(struct s2fs_inode_info),
					      0, (SLAB_RECLAIM_ACCOUNT|
						 SLAB_MEM_SPREAD|SLAB_ACCOUNT),
					      NULL);
	if (s2fs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	rcu_barrier();
	kmem_cache_destroy(s2fs_inode_cachep);
}

static int s2fs_fill_super(struct super_block *sb, void *data, int s2lent)
{
	struct buffer_head *bh;
	struct inode *root_inode;
	struct s2fs_sb_info *sbi;
	struct s2fs_sb *s2sb;
	int ret = -EINVAL;

	sbi = kzalloc(sizeof(struct s2fs_sb_info *), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	if (!sb_set_blocksize(sb, BLOCK_DEFAULT_SIZE))
		goto out_bad_hblock;

	bh = sb_bread(sb, S2FS_SUPER_BLOCK_NUMBER);
	if (!bh)
		goto out_bad_sb;

	s2sb = (struct s2fs_sb *)bh->b_data;

	printk(KERN_INFO "The s_magic number obtained in disk is: [%x]\n",
			sbi->s_magic);
	printk(KERN_INFO "There are %u inodes\n", sbi->s_ninodes);

	/* fill superblock informatin */
	sbi->s_s2sb = s2sb;
	sbi->s_sbh = bh;
	sbi->s_ninodes = s2sb->s_ninodes;
	sbi->s_magic = s2sb->s_magic;
	sbi->s_version = s2sb->s_version;

	sb->s_op = &s2fs_sops;

	/* Make root inode */
	ret = -ENOMEM;
	root_inode = s2fs_iget(sb, S2FS_ROOT_INO);
	if (IS_ERR(root_inode))
		goto out_no_root;

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
		goto out_no_root;

	printk(KERN_INFO "Fill superblock successfully\n");

	return 0;

out_no_root:
	printk("s2fs: get root inode failed\n");
	brelse(bh);
	goto out;
out_bad_sb:
	printk("s2fs: unable to read superblock\n");
	goto out;
out_bad_hblock:
	printk("s2fs: blocksize too small for device\n");
out:
	sb->s_fs_info = NULL;
	kfree(sbi);
	return ret;

}

static struct dentry *s2fs_mount(struct file_system_type *fs_type,
				 int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, s2fs_fill_super);
}

static struct file_system_type s2fs_fs_type = {
	.owner    = THIS_MODULE,
	.name     = "s2fs",
	.mount    = s2fs_mount,
	.kill_sb  = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("s2fs");

static int __init init_s2fs_fs(void)
{
	int err;

	err = init_inodecache();
	if (err)
		goto out1;

	err = register_filesystem(&s2fs_fs_type);
	if (err)
		goto out;

	return 0;
out:
	destroy_inodecache();
out1:
	return err;
}

static void __exit exit_s2fs_fs(void)
{
	unregister_filesystem(&s2fs_fs_type);
	destroy_inodecache();
}

module_init(init_s2fs_fs);
module_exit(exit_s2fs_fs);
MODULE_LICENSE("GPL");
