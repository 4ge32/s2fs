#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "s2fs.h"

static inline void dir_put_page(struct page *page)
{
	kunmap(page);
	put_page(page);
}

/*
 * Return the offset into page `page_nr' of the last valid
 * byte in that page, plus one.
 */
static unsigned
s2fs_last_byte(struct inode *inode, unsigned long page_nr)
{
	unsigned last_byte = PAGE_SIZE;

	if (page_nr == (inode->i_size >> PAGE_SHIFT))
		last_byte = inode->i_size & (PAGE_SIZE - 1);
	return last_byte;
}

static struct page * dir_get_page(struct inode *dir, unsigned long n)
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page = read_mapping_page(mapping, n, NULL);
	if (!IS_ERR(page))
		kmap(page);
	return page;
}

static inline void *s2fs_next_entry(void *de, struct s2fs_sbi_info *sbii)
{
	return (void*)((char*)de + 64);
}

static int s2fs_readdir(struct file *fp, struct dir_context *ctx)
{
	struct inode *inode = file_inode(fp);
	struct super_block *sbi = fp->f_inode->i_sb;
	struct s2fs_sbi_info *sbii = S2FS_SUPER(sbi);
	unsigned chunk_size = 64;
	unsigned long pos = ctx->pos;
	unsigned long offset;
	unsigned long npages = dir_pages(inode);
	unsigned long n;
	static int count;

	printk("DIR");
	printk("DIR: I_SIZE %llu", inode->i_size);
	printk("DIR: pos %lu", pos);

	return 0;
	printk("?HERE? %d", count++);
	ctx->pos = pos = ALIGN(pos, chunk_size);
	printk("?HERE? %d", count++);

	if (pos >= inode->i_size)
		return 0;

	offset = pos & ~PAGE_MASK;
	n = pos >> PAGE_SHIFT;

	printk("DIR: n %lu", n);
	printk("DIR: napges %lu", npages);

	for ( ; n < npages; n++, offset = 0) {
		char *p, *kaddr, *limit;
		struct page *page = dir_get_page(inode, n);
		printk("?HERE? %d", count++);

		if (IS_ERR(page))
			continue;
		kaddr = (char *)page_address(page);
		p = kaddr + offset;
		limit = kaddr + s2fs_last_byte(inode, n) - chunk_size;
		printk("?HERE? %d", count++);
		for ( ; p <= limit; p = s2fs_next_entry(p, sbii)) {
			const char *name;
			__u32 inumber;
			struct s2fs_dir_record *de = (struct s2fs_dir_record *)p;
			name = de->filename;
			inumber = de->inode_no;
			printk("DIR: %s", name);
			if (inumber) {
				unsigned l = strnlen(name, 100);
				if (!dir_emit(ctx, name, l, inumber, DT_UNKNOWN)) {
					dir_put_page(page);
					return 0;
				}
			}
			ctx->pos += chunk_size;
		}
		dir_put_page(page);
	}
	return 0;
}

const struct file_operations s2fs_dir_ops = {
	.iterate_shared = s2fs_readdir,
	//.llseek         = generic_file_llseek,
	//.read           = generic_read_dir,
};
