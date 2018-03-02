#include <linux/slab.h>
#include <linux/buffer_head.h>

#include "sifs.h"

static struct dentry *sifs_lookup(struct inode *parent_inode, struct dentry *child_dentry,
				  unsigned int flags)
{
	return NULL;
}

const struct inode_operations sifs_inode_ops = {
	.lookup = sifs_lookup,
};
