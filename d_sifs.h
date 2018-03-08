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

