#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

static int s2fs_readdir(struct file *fp, struct dir_context *ctx) {

	loff_t pos;
	struct super_block *sb;
	struct buffer_head *bh_inode;
	struct s2fs_inode *s2_inode;
	struct s2fs_inode *dir = S2FS_INODE(fp->f_path.dentry->d_inode);
	int i;

	pos = ctx->pos;
	sb = fp->f_inode->i_sb;
	if (pos)
		return 0;

	bh_inode = sb_bread(sb, S2FS_INODE_STORE_BLOCK_NUMBER);
	s2_inode = (struct s2fs_inode *)bh_inode->b_data;
	s2_inode += dir->inode_no;


	dir_emit_dots(fp, ctx);
	for (i = 0; i < dir->children_count; i++) {
		if (s2_inode->rec == NULL) {
			s2fs_get_inode_record(sb, s2_inode);
		}
		if (s2_inode->rec != NULL) {
			printk("READDIR: %s\n", s2_inode->rec->filename);
			printk("READDIR: %p\n", &s2_inode->rec->filename);
			printk("READDIR: %d\n", s2_inode->rec->inode_no);
		}
		if (!s2_inode->valid) {
			s2_inode++;
			continue;
		}
		if (!dir_emit(ctx, s2_inode->rec->filename, strlen(s2_inode->rec->filename),
			                   s2_inode->rec->inode_no, DT_UNKNOWN))
			goto out;

		ctx->pos += sizeof(struct s2fs_dir_record);
		pos += sizeof(struct s2fs_dir_record);
		s2_inode++;
	}


out:
	brelse(bh_inode);

	return 0;
}

const struct file_operations s2fs_dir_ops = {
	.iterate        = s2fs_readdir,
};
