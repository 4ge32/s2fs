#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#include "sifs.h"

#define NEXT 1
#define END  0

static void write_superblock(int fd)
{
	ssize_t ret;

	struct sifs_sb sb = {
		.version = 0,
		.magic = SIFS_MAGIC,
		.inodes = 3,
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

	root_inode.mode = S_IFDIR | 0777;
	root_inode.inode_no = SIFS_ROOTDIR_INODE_NUMBER;
	root_inode.children_count = 2;

	ret = write(fd, &root_inode, sizeof(root_inode));
	if (ret != sizeof(root_inode)) {
		printf("The inode store was not write properly.\n");
	}

	return ret;

}

static ssize_t write_inode(int fd, struct sifs_inode inode, ssize_t size, int next)
{
	ssize_t ret;

	ret = write(fd, &inode, sizeof(inode));
	if (ret != sizeof(inode)) {
		printf("The inode store was not write properly.\n");
	}

	if (next)
		return ret;

	if (size + ret != BLOCK_DEFAULT_SIZE) {
		off_t skip = BLOCK_DEFAULT_SIZE - (size + ret);
		printf("%ld\n", skip + ret + size);
		lseek(fd, skip, SEEK_CUR);
	}
}

static ssize_t write_record(int fd, struct sifs_dir_record record, ssize_t size, int next)
{
	ssize_t ret;

	ret = write(fd, &record, sizeof(record));
	if (ret != sizeof(record)) {
		printf("The inode store was not write properly.\n");
	}

	if (next)
		return ret;

	if (size + ret != BLOCK_DEFAULT_SIZE) {
		off_t skip = BLOCK_DEFAULT_SIZE - (size + ret);
		printf("%ld\n", skip + ret + size);
		lseek(fd, skip, SEEK_CUR);
	}
}

static void write_block(int fd, char *block, size_t len)
{
	ssize_t ret;

	ret = write(fd, block, len);
	if (ret != len) {
		printf("ERROR: Writing file body.\n");
	}
	if (ret != BLOCK_DEFAULT_SIZE) {
		off_t skip = BLOCK_DEFAULT_SIZE - ret;
		printf("%ld\n", skip + ret);
		lseek(fd, skip, SEEK_CUR);
	}
}

int main(int argc, char *argv[])
{
	int fd;
	char file_content[]  = "Sehen Sie mich! Sehen Sie mich! Das Monstrum in meinem Selbst ist So gross geworden!\n";
	char file_content2[] = "Heute ist die beste Zeit\n";
	size_t f_size = sizeof(file_content);
	size_t f_size2 = sizeof(file_content2);
	struct sifs_dir_record record = {
		.filename = "TEST",
		.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 1,
	};
	struct sifs_inode inode = {
		.mode = S_IFREG | 0666,
		.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 1,
		.data_block_number = SIFS_ROOTDIR_DATABLOCK_NUMBER,
		.file_size = f_size,
	};
	struct sifs_dir_record record2 = {
		.filename = "MEIG",
		.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 2,
	};
	struct sifs_inode inode2 = {
		.mode = S_IFREG | 0666,
		.inode_no = SIFS_ROOTDIR_INODE_NUMBER + 2,
		.data_block_number = SIFS_ROOTDIR_DATABLOCK_NUMBER + 1,
		.file_size = f_size2,
	};

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
	size += write_inode(fd, inode, 0, NEXT);
	write_inode(fd, inode2, size, END);
	size = write_record(fd, record, 0, NEXT);
	write_record(fd, record2, size, END);
	write_block(fd, file_content, f_size);
	write_block(fd, file_content2, f_size2);

	close(fd);

	return 0;
}
