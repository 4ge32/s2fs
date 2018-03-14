#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "sifs.h"

static int sifs_readdir(struct file *fp, struct dir_context *ctx) {

	loff_t pos;
	struct super_block *sb;
	struct buffer_head *bh_inode;
	struct sifs_inode *si_inode;
	struct buffer_head *bh_record;
	struct sifs_dir_record *record;
	int i;

	pos = ctx->pos;
	sb = fp->f_inode->i_sb;
	if (pos)
		return 0;

	bh_inode = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER);
	si_inode = (struct sifs_inode *)bh_inode->b_data;

	bh_record = sb_bread(sb, SIFS_INODE_STORE_BLOCK_NUMBER + 1);
	record = (struct sifs_dir_record *)bh_record->b_data;


	dir_emit_dots(fp, ctx);
	for (i = 0; i < si_inode->children_count; i++) {
		if (!dir_emit(ctx, record->filename, strlen(record->filename),
			                   record->inode_no, DT_UNKNOWN))
			goto out;
		ctx->pos += sizeof(struct sifs_dir_record);
		pos += sizeof(struct sifs_dir_record);
		record++;
	}


out:
	brelse(bh_inode);
	brelse(bh_record);

	return 0;
}

const struct file_operations sifs_dir_ops = {
	.iterate        = sifs_readdir,
};
