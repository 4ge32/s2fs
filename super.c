/* ISSUE 1:
 * How do I change BLOCK SIZE?
 * sb_bread will be failure when block_size set except 4096.
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include "sifs.h"

struct kmem_cache *sifs_inode_cachep;

static const struct super_operations sifs_sops = {
};

static int init_inodecache(void)
{
	sifs_inode_cachep = kmem_cache_create("sifs_inode_cache",
					      sizeof(struct sifs_inode),
					      0, (SLAB_RECLAIM_ACCOUNT|
						 SLAB_MEM_SPREAD|SLAB_ACCOUNT),
					      NULL);
	if (sifs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	rcu_barrier();
	kmem_cache_destroy(sifs_inode_cachep);
}

static int sifs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct sifs_sb *si_sb;
	struct sifs_sb_info *sbi;
	struct buffer_head *bh;
	struct inode *root_inode;
	int ret = -EPERM;


	bh = sb_bread(sb, SIFS_SUPER_BLOCK_NUMBER);
	if (!bh)
		goto out_bad_sb;

	si_sb = (struct sifs_sb *)bh->b_data;

	printk(KERN_INFO "The magic number obtained in disk is: [%llx]\n",
			si_sb->magic);
	printk(KERN_INFO "There are %llu inodes\n", si_sb->inodes);

	if (unlikely(si_sb->magic != SIFS_MAGIC))
		goto magic_mismatch;

	if (unlikely(si_sb->block_size != BLOCK_DEFAULT_SIZE))
		goto bsize_mismatch;
	printk(KERN_INFO "block size is %llu\n", si_sb->block_size);
	printk(KERN_INFO "block size is %lu\n", sb->s_blocksize);

	sbi = kzalloc(sizeof(struct sifs_sb_info), GFP_KERNEL);

	sbi->s_bh = bh;
	sbi->si_sb = si_sb;
	sbi->inodes = si_sb->inodes;

	sb->s_fs_info = sbi;
	sb->s_magic = SIFS_MAGIC;
	sb->s_blocksize = BLOCK_DEFAULT_SIZE;
	sb->s_op = &sifs_sops;

	root_inode = new_inode(sb);
	root_inode->i_ino = SIFS_ROOTDIR_INODE_NUMBER;
	inode_init_owner(root_inode, NULL, S_IFDIR);
	root_inode->i_sb = sb;
	root_inode->i_op = &sifs_inode_ops;
	root_inode->i_fop = &sifs_dir_ops;
	root_inode->i_private = sifs_get_inode(sb, SIFS_ROOTDIR_INODE_NUMBER);
	if (SIFS_INODE(root_inode) == NULL)
		return -ENOMEM;
	root_inode->i_mode = SIFS_INODE(root_inode)->mode;

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
	printk("sifs: unable to read superblock\n");
out:
	return ret;

}

static struct dentry *sifs_mount(struct file_system_type *fs_type,
				 int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, sifs_fill_super);
}

static struct file_system_type sifs_fs_type = {
	.owner    = THIS_MODULE,
	.name     = "sifs",
	.mount    = sifs_mount,
	.kill_sb  = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("sifs");

static int __init init_sifs_fs(void)
{
	int err;

	err = init_inodecache();
	if (err)
		goto out1;

	err = register_filesystem(&sifs_fs_type);
	if (err)
		goto out;

	return 0;
out:
	destroy_inodecache();
out1:
	return err;
}

static void __exit exit_sifs_fs(void)
{
	unregister_filesystem(&sifs_fs_type);
	destroy_inodecache();
}

module_init(init_sifs_fs);
module_exit(exit_sifs_fs);
MODULE_LICENSE("GPL");
