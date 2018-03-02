#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#include "sifs.h"

static void write_superblock(int fd)
{
	ssize_t ret;

	struct sifs_sb sb = {
		.version = 0,
		.magic = SIFS_MAGIC,
		.inodes = 1,
		.block_size = BLOCK_DEFAULT_SIZE,
	};

	ret = write(fd, &sb, sizeof(sb));
	if (ret != BLOCK_DEFAULT_SIZE) {
		printf("bytes written [%ld]\n", ret);
	}
}

static void write_root_inode(int fd)
{
	ssize_t ret;
	struct sifs_inode root_inode;

	root_inode.mode = S_IFDIR;
	root_inode.inode_no = SIFS_ROOTDIR_INODE_NUMBER;

	ret = write(fd, &root_inode, sizeof(root_inode));
	if (ret != sizeof(root_inode)) {
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
	write_root_inode(fd);
	//
	close(fd);

	return 0;
}
