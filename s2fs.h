#ifndef _LINUX_S2FS_FS_H
#define _LINUX_S2FS_FS_H

#include <linux/types.h>
#include <linux/magic.h>

/*
 * The super simple filesystem constants/structures
 */

#define S2FS_ROOT_INO 1


#define S2FS_MAGIC 0xEFF10
#define BLOCK_DEFAULT_SIZE 4096
#define S2FS_FILENAME_MAX_LEN 256
#define S2FS_FILE_MAXSIZE 512

#define S2FS_SUPER_BLOCK_NUMBER 0
#define S2FS_INODE_STORE_BLOCK_NUMBER 1
#define S2FS_RECORD_BLOCK_NUMBER 2
#define S2FS_ROOTDIR_DATABLOCK_NUMBER 3

struct s2fs_dir_record {
	char filename[S2FS_FILENAME_MAX_LEN];
	uint32_t inode_no;
};

/*
 * This is the original s2fs inode layout on disk.
 */
struct s2fs_inode {
	__u16 i_mode;
	__u16 i_uid;
	__u32 i_size;
	__u32 i_time;
	__u8  i_gid;
	__u8  i_nlinks;
	__u16 i_zone[9];
};

/*
 * s2fs super-block data on disk
 */
struct s2fs_sb {
	__le16 s_version;
	__le32 s_magic;
	__le32 s_ninodes;
	__le32 freeblocks_count;
	__le64 block_size;
};

struct s2fs_inode_info {
	mode_t mode;
	uint8_t valid;
	uint32_t inode_no;
	uint32_t data_block_number;
	uint32_t children_count;
	uint32_t file_size;
	uint32_t ref_count;
	struct s2fs_dir_record *rec;
	struct inode *vfs_inode;
};

/*
 * s2fs super-block data in memory
 */
struct s2fs_sbi_info {
	uint16_t s_version;
	uint32_t s_magic;
	uint32_t s_ninodes;
	uint32_t freeblocks_count;
	uint64_t block_size;
	struct s2fs_sb *s_s2sb;
	struct buffer_head *s_sbh;
};

#ifndef MKFS
enum {
	DIRECTORY,
	REGFILE,
};

extern struct kmem_cache *s2fs_inode_info_cachep;

extern const struct file_operations s2fs_dir_ops;
extern const struct file_operations s2fs_file_ops;
extern const struct inode_operations s2fs_inode_ops;
extern const struct address_space_operations s2fs_aops;

struct s2fs_inode_info *s2fs_get_inode(struct super_block *, uint64_t);
struct inode *s2fs_iget(struct super_block *, int);
int s2fs_inode_info_save(struct super_block *, struct s2fs_inode_info *);
int s2fs_get_inode_record(struct super_block *, struct s2fs_inode_info *);

loff_t s2fs_file_llseek(struct file *, loff_t , int);

static inline struct s2fs_sbi_info *S2FS_SUPER(struct super_block *sbi) {
	return sbi->s_fs_info;
}

static inline struct s2fs_inode_info *S2FS_INODE(struct inode *inode) {
	return inode->i_private;
}
#endif
#endif
