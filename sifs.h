#define SIFS_MAGIC 0xEFF10
#define BLOCK_DEFAULT_SIZE 4096
#define SIFS_FILENAME_MAX_LEN 256

#define SIFS_START_INO 10

#define SIFS_SUPER_BLOCK_NUMBER 0
#define SIFS_INODE_STORE_BLOCK_NUMBER 1

#define SIFS_ROOTDIR_INODE_NUMBER 1

#define SIFS_ROOTDIR_DATABLOCK_NUMBER 3

#define SIFS_FILE_MAXNUM 100

extern struct kmem_cache *sifs_inode_cachep;

struct sifs_dir_record {
	char filename[SIFS_FILE_MAXNUM];
	uint64_t inode_no;
};

struct sifs_inode {
	mode_t mode;
	uint64_t inode_no;
	uint64_t data_block_number;
	uint64_t file_size;
	uint64_t children_count;
};

/*
 * sifs super-block data on disk
 */
struct sifs_sb {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes;
	char padding[4060];
};

/*
 * sifs super-block data in memory
 */
struct sifs_sb_info {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes;
	struct buffer_head *s_bh;
	struct sifs_sb *si_sb;
};

#ifndef MKFS

extern const struct file_operations sifs_dir_ops;
extern const struct file_operations sifs_file_ops;
extern const struct inode_operations sifs_inode_ops;

struct sifs_inode *sifs_get_inode(struct super_block *sb, uint64_t inode_no);
struct inode *sifs_iget(struct super_block *sb, int ino);

static inline struct sifs_sb *SIFS_SUPER(struct super_block *sb) {
	return sb->s_fs_info;
}

static inline struct sifs_inode *SIFS_INODE(struct inode *inode) {
	return inode->i_private;
}
#endif
