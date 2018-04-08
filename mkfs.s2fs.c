#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <linux/types.h>

#include "s2fs.h"

#define true  1
#define false 0

/* mke2fs 1.43.5 (04-Aug-2017)
 * 	Discarding device blocks: done
 * 	Creating filesystem with 400 1k blocks and 56 inodes
 *
 * 	Allocating group tables: done
 * 	Writing inode tables: done
 * 	Writing superblocks and filesystem accounting information: done
 */



static void block_iterator()
{
	return;
}

static int write_superblock(int fd)
{
	ssize_t ret;
	struct s2fs_sb_info sbi = {
		.s_version      = 0,
		.s_magic        = S2FS_MAGIC,
		.s_ninodes = 3,
		.block_size   = BLOCK_DEFAULT_SIZE,
	};

	block_iterator();

	printf("Writing Superblock\n");

	ret = write(fd, &sbi, sizeof(sbi));
	if (ret != sizeof(sbi)) {
		printf("The super block was not written properly\n");
		return 0;
	}

	return 1;
}

static int write_rootinode(int fd)
{
	ssize_t ret;
	struct s2fs_inode_info rootinode = {
		rootinode.mode           = S_IFDIR | 0777,
		rootinode.valid          = true,
		rootinode.inode_no       = S2FS_ROOT_INO,
		rootinode.children_count = 2,
	};

	printf("Writing Root inode\n");

	ret = write(fd, &rootinode, sizeof(rootinode));
	if (ret != sizeof(rootinode)) {
		printf("The root inode was not written properly.\n");
		return 0;
	}

	return 1;

}

static int write_inode(int fd, struct s2fs_inode_info inode)
{
	ssize_t ret;

	printf("Writing inode\n");

	ret = write(fd, &inode, sizeof(inode));
	if (ret != sizeof(inode)) {
		printf("The inode was not written properly.\n");
		return 0;
	}

	return 1;
}

static int write_record(int fd, struct s2fs_dir_record record)
{
	ssize_t ret;

	printf("Writing entry\n");

	ret = write(fd, &record, sizeof(record));
	if (ret != sizeof(record)) {
		printf("The entry was not written properly.\n");
		return 0;
	}

	return 1;
}

static int write_block(int fd, char *block, size_t len)
{
	ssize_t ret;

	printf("Writing file data\n");

	ret = write(fd, block, len);

	if (ret != len) {
		printf("The file data was not written properly.\n");
		return 0;
	}

	if (ret != BLOCK_DEFAULT_SIZE) {
		off_t skip = BLOCK_DEFAULT_SIZE - ret;
		lseek(fd, skip, SEEK_CUR);
	}

	return 1;
}

int main(int argc, char *argv[])
{
	int fd;
	/*
	 * Inital contents for test during development
	 */
	char file_content[]  = "Sehen Sie mich! Sehen Sie mich! Das Monstrum in meinem Selbst ist So gross geworden!\n";
	char file_content2[] = "Heute ist die beste Zeit\n";

	size_t f_size = strlen(file_content);
	size_t f_size2 = strlen(file_content2);

	struct s2fs_dir_record record = {
		.filename = "TEST",
		.inode_no = S2FS_ROOT_INO + 1,
	};
	struct s2fs_inode_info inode = {
		.mode = S_IFREG | 0666,
		.inode_no = S2FS_ROOT_INO + 1,
		.data_block_number = S2FS_ROOTDIR_DATABLOCK_NUMBER,
		.file_size = f_size,
		.valid = true,
		.ref_count = 0,
	};

	struct s2fs_dir_record record2 = {
		.filename = "MEIG",
		.inode_no = S2FS_ROOT_INO + 2,
	};
	struct s2fs_inode_info inode2 = {
		.mode = S_IFREG | 0666,
		.inode_no = S2FS_ROOT_INO + 2,
		.data_block_number = S2FS_ROOTDIR_DATABLOCK_NUMBER + 1,
		.file_size = f_size2,
		.valid  = true,
		.ref_count = 0,
	};


	if (argc != 2) {
		printf("Usage: mkfs.s2fs <device>\n");
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("unable to open the device");
		return -1;
	}

	lseek(fd, 0, SEEK_SET);
	if (!write_superblock(fd))
		goto out;

	lseek(fd, 0x1000, SEEK_SET);
	if (!write_rootinode(fd))
		goto out;
	/*
	if (!write_inode(fd, inode))
		goto out;
	if (!write_inode(fd, inode2))
		goto out;

	lseek(fd, 0x2000, SEEK_SET);
	if (!write_record(fd, record))
		goto out;
	if (!write_record(fd, record2))
		goto out;

	lseek(fd, 0x3000, SEEK_SET);
	if (!write_block(fd, file_content, f_size))
		goto out;
	if (!write_block(fd, file_content2, f_size2))
		goto out;
	*/

	printf("SUCCESS\n");
	close(fd);
	return 0;
out:
	printf("ERROR\n");
	close(fd);

	return 0;
}
