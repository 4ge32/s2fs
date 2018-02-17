#include <linux/module.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "sifs.h"


static struct kmem_cache *sifs_inode_cachep;

static int init_inodecache(void)
{
	sifs_inode_cachep = kmem_cache_create("sifs_inode_cache",
					      sizeof(struct sifs_inode_info),
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
	int ret = -EINVAL;

	bh = sb_bread(sb, SIFS_SUPER_BLOCK_NUMBER);
	if (!bh)
		goto out_bad_sb;

	si_sb = (struct sifs_sb *)bh->b_data;

	printk(KERN_INFO "The magic number obtained in disk is: [%llx]\n",
			si_sb->magic);

	if (unlikely(si_sb->magic != SIFS_MAGIC)) {
		printk(KERN_ERR "MAGIC NUMBER MISMATCH\n");
		brelse(bh);
		return ret;
	}

	return ret;

	sbi = kzalloc(sizeof(struct sifs_sb_info), GFP_KERNEL);

	sbi->s_bh = bh;
	sbi->si_sb = si_sb;
	sb->s_fs_info = sbi;

	sb->s_magic = SIFS_MAGIC;

	return 0;
out_bad_sb:
	printk("sifs: unable to read superblock\n");
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
