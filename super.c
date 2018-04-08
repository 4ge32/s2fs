/* ISSUE 1:
 * How do I change BLOCK SIZE?
 * sb_bread will be failure when block_size set except 4096.
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

struct kmem_cache *s2fs_inode_info_cachep;

static const struct super_operations s2fs_sops = {
};

static int init_inodecache(void)
{
	s2fs_inode_info_cachep = kmem_cache_create("s2fs_inode_info_cache",
					      sizeof(struct s2fs_inode_info),
					      0, (SLAB_RECLAIM_ACCOUNT|
						 SLAB_MEM_SPREAD|SLAB_ACCOUNT),
					      NULL);
	if (s2fs_inode_info_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	rcu_barrier();
	kmem_cache_destroy(s2fs_inode_info_cachep);
}

static int s2fs_fill_super(struct super_block *sb, void *data, int s2lent)
{
	struct buffer_head *bh;
	struct inode *root_inode;
	struct s2fs_sbi_info *sbi;
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

	//if (unlikely(sbi->s_magic != S2FS_MAGIC))
	//	goto s_magic_mismatch;

	//if (unlikely(sbi->block_size != BLOCK_DEFAULT_SIZE))
	//	goto bsize_mismatch;

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
