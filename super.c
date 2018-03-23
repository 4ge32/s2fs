/* ISSUE 1:
 * How do I change BLOCK SIZE?
 * sb_bread will be failure when block_size set except 4096.
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

struct kmem_cache *s2fs_inode_cachep;

static const struct super_operations s2fs_sops = {
};

static int init_inodecache(void)
{
	s2fs_inode_cachep = kmem_cache_create("s2fs_inode_cache",
					      sizeof(struct s2fs_inode),
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
	struct s2fs_sb *s2_sb;
	struct s2fs_sb *sbi;
	struct buffer_head *bh;
	struct inode *root_inode;
	int ret = -EPERM;


	bh = sb_bread(sb, S2FS_SUPER_BLOCK_NUMBER);
	if (!bh)
		goto out_bad_sb;

	s2_sb = (struct s2fs_sb *)bh->b_data;

	printk(KERN_INFO "The magic number obtained in disk is: [%x]\n",
			s2_sb->magic);
	printk(KERN_INFO "There are %u inodes\n", s2_sb->inodes_count);

	if (unlikely(s2_sb->magic != S2FS_MAGIC))
		goto magic_mismatch;

	if (unlikely(s2_sb->block_size != BLOCK_DEFAULT_SIZE))
		goto bsize_mismatch;
	printk(KERN_INFO "block size is %llu\n", s2_sb->block_size);
	printk(KERN_INFO "block size is %lu\n", sb->s_blocksize);

	sbi = kzalloc(sizeof(struct s2fs_sb), GFP_KERNEL);
	sbi = s2_sb;

	//sbi->s_bh = bh;
	//sbi->s2_sb = sbi;
	sbi->inodes_count = s2_sb->inodes_count;

	sb->s_fs_info = s2_sb;
	sb->s_magic = S2FS_MAGIC;
	sb->s_blocksize = BLOCK_DEFAULT_SIZE;
	sb->s_op = &s2fs_sops;

	root_inode = new_inode(sb);
	root_inode->i_ino = S2FS_ROOTDIR_INODE_NUMBER;
	inode_init_owner(root_inode, NULL, S_IFDIR);
	root_inode->i_sb = sb;
	root_inode->i_op = &s2fs_inode_ops;
	root_inode->i_fop = &s2fs_dir_ops;
	root_inode->i_private = s2fs_get_inode(sb, S2FS_ROOTDIR_INODE_NUMBER);
	if (S2FS_INODE(root_inode) == NULL)
		return -ENOMEM;
	root_inode->i_mode = S2FS_INODE(root_inode)->mode;

	if (!S_ISDIR(root_inode->i_mode)) {
		iput(root_inode);
		return -EPERM;
	}

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root) {
		ret = -ENOMEM;
		goto release;
	}

	printk(KERN_INFO "fill superblock successfully\n");
	return 0;

bsize_mismatch:
	printk(KERN_ERR "BLOCK SIZE MISMATCH\n");
	goto out;
magic_mismatch:
	printk(KERN_ERR "MAGIC NUMBER MISMATCH\n");
	goto out;
release:
	brelse(bh);
	goto out;
out_bad_sb:
	printk("s2fs: unable to read superblock\n");
out:
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
