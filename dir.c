#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "sifs.h"

static int sifs_readdir(struct file *fp, struct dir_context *ctx) {

	loff_t pos;
	struct super_block *sb;
	struct buffer_head *bh_inode;
	struct sifs_inode *si_inode;
	struct sifs_inode *dir = SIFS_INODE(fp->f_path.dentry->d_inode);
	int i;
	int count = 0;

	pos = ctx->pos;
	sb = fp->f_inode->i_sb;
	if (pos)
		return 0;

	bh_inode = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER);
	si_inode = (struct sifs_inode *)bh_inode->b_data;
	si_inode += dir->inode_no;


	dir_emit_dots(fp, ctx);
	for (i = 0; i < dir->children_count; i++) {
		if (si_inode->rec == NULL) {
			printk("READDIR: IF NULL %d", count++);
			sifs_get_inode_record(sb, si_inode);
		}
		if (si_inode->rec != NULL) {
			printk("READDIR: %s\n", si_inode->rec->filename);
			printk("READDIR: %p\n", &si_inode->rec->filename);
			printk("READDIR: %lld\n", si_inode->rec->inode_no);
		}
		if (!si_inode->valid) {
			si_inode++;
			continue;
		}
		if (!dir_emit(ctx, si_inode->rec->filename, strlen(si_inode->rec->filename),
			                   si_inode->rec->inode_no, DT_UNKNOWN))
			goto out;

		ctx->pos += sizeof(struct sifs_dir_record);
		pos += sizeof(struct sifs_dir_record);
		si_inode++;
	}


out:
	brelse(bh_inode);

	return 0;
}

const struct file_operations sifs_dir_ops = {
	.iterate        = sifs_readdir,
};
