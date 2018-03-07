#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#include "d_sifs.h"

static void write_superblock(int fd)
{
	ssize_t ret;

	struct sifs_sb sb = {
		.version = 0,
		.magic = SIFS_MAGIC,
		.inodes = 2,
		.block_size = BLOCK_DEFAULT_SIZE,
	};

	ret = write(fd, &sb, sizeof(sb));
	if (ret != BLOCK_DEFAULT_SIZE) {
		printf("bytes written [%ld]\n", ret);
	}
}

static ssize_t write_root_inode(int fd)
{
	ssize_t ret;
	struct sifs_inode root_inode;

	root_inode.mode = S_IFDIR | 0755;
	root_inode.inode_no = SIFS_ROOTDIR_INODE_NUMBER;
	root_inode.children_count = 1;

	ret = write(fd, &root_inode, sizeof(root_inode));
	if (ret != sizeof(root_inode)) {
		printf("The inode store was not write properly.\n");
	}

	return ret;

}

static void write_inode(int fd, ssize_t size)
{
	ssize_t ret;
	struct sifs_inode inode;

	inode.mode = S_IFREG | 0666;
	inode.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 1;

	ret = write(fd, &inode, sizeof(inode));
	if (ret != sizeof(inode)) {
		printf("The inode store was not write properly.\n");
	}

	if (size + ret != BLOCK_DEFAULT_SIZE) {
		printf("%ld\n",  size + ret);
		off_t skip = BLOCK_DEFAULT_SIZE - (size + ret);
		printf("%ld\n", skip);
		lseek(fd, skip, SEEK_CUR);
	}
}

static void write_record(int fd)
{
	ssize_t ret;
	struct sifs_dir_record record = {
		.filename = "TEST",
		.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 1,
	};

	ret = write(fd, &record, sizeof(record));
	if (ret != sizeof(record)) {
		printf("The inode store was not write properly.\n");
	}
}

int main(int argc, char *argv[])
{
	int fd;

	if (argc != 2) {
		printf("Usage: mkfs.sifs <device>\n");
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("unable to open the device");
		return -1;
	}

	lseek(fd, 0, SEEK_SET);

	write_superblock(fd);
	ssize_t size = write_root_inode(fd);
	write_inode(fd, size);
	write_record(fd);

	close(fd);

	return 0;
}
