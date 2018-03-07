#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "sifs.h"


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
		if (si_inode->inode_no == inode_no) {
			inode_buf = kmem_cache_alloc(sifs_inode_cachep, GFP_KERNEL);
			memcpy(inode_buf, si_inode, sizeof(*inode_buf));
		}
		si_inode++;
	}
	brelse(bh);
	return inode_buf;
}

struct inode *sifs_iget(struct super_block *sb, int ino)
{
	struct inode *inode;
	struct sifs_inode *si_inode;

	si_inode = sifs_get_inode(sb, ino);

	inode = new_inode(sb);
	inode->i_ino = ino;
	inode->i_sb = sb;
	inode->i_op = &sifs_inode_ops;

	if (S_ISDIR(si_inode->mode))
		inode->i_fop = &sifs_dir_ops;
	else if (S_ISREG(si_inode->mode))
		inode->i_fop = &sifs_file_ops;
	else
		printk("Unknow inode type\n");

	inode->i_atime = inode->i_mtime = inode->i_ctime =
		current_time(inode);
	inode->i_private = si_inode;

	return inode;
}
