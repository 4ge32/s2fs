#include <linux/fs.h>

#define SIFS_MAGIC 0xEFF10
#define BLOCK_DEFAULT_SIZE 512
#define SIFS_FILENAME_MAX_LEN 256

#define SIFS_START_INO 10

#define SIFS_SUPER_BLOCK_NUMBER 0

struct sifs_inode_info {
	mode_t mode;
	uint64_t inode_no;
};


/*
 * sifs super-block data on disk
 */
struct sifs_sb {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	char padding[488];
};

/*
 * sifs super-block data in memory
 */
struct sifs_sb_info {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	struct buffer_head *s_bh;
	struct sifs_sb *si_sb;
};
