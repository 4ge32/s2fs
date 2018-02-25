#define SIFS_MAGIC 0xEFF10
#define BLOCK_DEFAULT_SIZE 512
#define SIFS_FILENAME_MAX_LEN 256

#define SIFS_START_INO 10
#define SIFS_ROOTDIR_INODE_NUMBER 1
#define SIFS_INODE_STORE_BLOCK_NUMBER 1

#define SIFS_SUPER_BLOCK_NUMBER 0

#define SIFS_ROOTDIR_DATABLOCK_NUMBER 4

#define SIFS_FILE_MAXNUM 100

struct sifs_dir_record {
	char filename[SIFS_FILE_MAXNUM];
	uint64_t inode_no;
};

struct sifs_inode {
	mode_t mode;
	uint64_t inode_no;
	uint64_t data_block_number;
};

/*
 * sifs super-block data on disk
 */
struct sifs_sb {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes;
	char padding[480];
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

extern const struct file_operations sifs_dir_ops;
extern const struct inode_operations sifs_inode_ops;
