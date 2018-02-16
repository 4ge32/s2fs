#include <linux/fs.h>

#define SIFS_MAGIC 0xEFF10
#define SIFS_DEFAULT_BLOCK_SIZE 4096
#define SIFS_FILENAME_MAX_LEN 256

#define SIFS_START_INO 10

#define SIFS_SUPER_BLOCK_NUMBER 1

struct sifs_inode_info {
	mode_t mode;
	uint64_t inode_no;
};


/*
 * sifs super-block data in memory
 */
struct sifs_sb_info {
};
