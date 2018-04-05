#define S2FS_MAGIC 0xEFF10
#define BLOCK_DEFAULT_SIZE 4096
#define S2FS_FILENAME_MAX_LEN 256
#define S2FS_FILE_MAXSIZE 512

#define S2FS_SUPER_BLOCK_NUMBER 0
#define S2FS_INODE_STORE_BLOCK_NUMBER 1
#define S2FS_RECORD_BLOCK_NUMBER 2
#define S2FS_ROOTDIR_DATABLOCK_NUMBER 3

#define S2FS_ROOTDIR_INODE_NUMBER 1

struct s2fs_dir_record {
	char filename[S2FS_FILENAME_MAX_LEN];
	uint32_t inode_no;
};

struct s2fs_inode {
	mode_t mode;
	uint8_t valid;
	uint32_t inode_no;
	uint32_t data_block_number;
	uint32_t children_count;
	uint32_t file_size;
	uint32_t ref_count;
	struct s2fs_dir_record *rec;
};

/*
 * s2fs super-block data on disk
 */
struct s2fs_sb {
	__le16 version;
	__le32 magic;
	__le32 inodes_count;
	__le32 freeblocks_count;
	__le64 block_size;
};

/*
 * s2fs super-block data in memory
 * s2fs_sb_info is not used now...
 * This will be planed to use it.
 */
struct s2fs_sb_info {
	uint16_t version;
	uint32_t magic;
	uint32_t inodes_count;
	struct buffer_head *s_bh;
	struct s2fs_sb *s2_sb;
};

#ifndef MKFS
enum {
	DIRECTORY,
	REGFILE,
};

extern struct kmem_cache *s2fs_inode_cachep;

extern const struct file_operations s2fs_dir_ops;
extern const struct file_operations s2fs_file_ops;
extern const struct inode_operations s2fs_inode_ops;
extern const struct address_space_operations s2fs_aops;

struct s2fs_inode *s2fs_get_inode(struct super_block *, uint64_t);
struct inode *s2fs_iget(struct super_block *, int);
int s2fs_inode_save(struct super_block *, struct s2fs_inode *);
int s2fs_get_inode_record(struct super_block *, struct s2fs_inode *);

loff_t s2fs_file_llseek(struct file *, loff_t , int);

static inline struct s2fs_sb *S2FS_SUPER(struct super_block *sb) {
	return sb->s_fs_info;
}

static inline struct s2fs_inode *S2FS_INODE(struct inode *inode) {
	return inode->i_private;
}
#endif
