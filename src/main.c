#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/ioctl.h>
#include<linux/fs.h>

#define SIGNATURE_1	0x6873616865657274ULL
#define SIGNATURE_2	0x656c6966736e6962ULL

#define REGION_TYPE_NULL	0x00
#define REGION_TYPE_NODES	0x01
#define REGION_TYPE_RAWDATA	0x02
#define REGION_TYPE_NODE_EXTEND	0x03

typedef uint64_t index_t;

typedef struct _node Node;

struct _node {
	uint64_t	value;
	index_t		parent;
	index_t		left;
	index_t		right;
	index_t		lchild;
	index_t		rchild;
	uint64_t	lcount;
	uint64_t	rcount;

	uint64_t	lupval;
	uint64_t	rupval;
	uint8_t		is_real;
	uint8_t		npad1[47];
};

typedef struct _metadata Metadata;

struct _metadata {
	uint64_t	sig1;
	uint64_t	sig2;
	index_t		root;
	uint64_t	region_count;
	uint64_t	region_size;
	uint64_t	region_index;
	uint64_t	size_filled;
	uint64_t	size_exfilled;
	uint8_t		region_type;
	uint8_t		mpad1[63];
};

typedef union _block Block;

union _block {
	char			r[128];
	struct {
		uint64_t	value;
		index_t		parent;
		index_t		left;
		index_t		right;
		index_t		lchild;
		index_t		rchild;
		uint64_t	lcount;
		uint64_t	rcount;
		uint64_t	lupval;
		uint64_t	rupval;
		uint8_t		is_real;
		uint8_t		npad1[47];
	};
	struct {
		uint64_t	sig1;
		uint64_t	sig2;
		index_t		root;
		uint64_t	region_count;
		uint64_t	region_size;
		uint64_t	region_index;
		uint64_t	size_filled;
		uint64_t	size_exfilled;
		uint8_t		region_type;
		uint8_t		mpad1[63];
	};
};

Node *makeroot (Node *root) {
	root->parent = -1;
	root->left = -1;
	root->right = -1;
	root->lchild = -1;
	root->rchild = -1;
	root->lcount = 0;
	root->rcount = 0;
	root->lupval = 0;
	root->rupval = 0xffffffffffffffffULL;
	root->is_real = 0;
	return root;
}

index_t search (Block *mmptr, index_t root, uint64_t value) {
	index_t curr = root;
	while (1) {
		if (value > mmptr[curr].value) {
			if (mmptr[curr].rchild == -1) { return -1; }
			curr = mmptr[curr].rchild;
		} else if (value < mmptr[curr].value) {
			if (mmptr[curr].lchild == -1) { return -1; }
			curr = mmptr[curr].lchild;
		} else { return curr; }
	}
	return -1;
}

index_t insert (Block *mmptr, index_t root, index_t value) {
	index_t curr = root;
	index_t left = -1;
	index_t right = -1;
	uint64_t lupval = mmptr[root].lupval;
	uint64_t rupval = mmptr[root].rupval;
	while (1) {
		if (mmptr[value].value > mmptr[curr].value) {
			left = curr;
			lupval = mmptr[curr].value;
			mmptr[curr].rcount += 1;
			if (mmptr[curr].rchild == -1) {
				mmptr[curr].rchild = value;
				mmptr[value].parent = curr;
				mmptr[value].left = left;
				mmptr[value].right = right;
				mmptr[value].lchild = -1;
				mmptr[value].rchild = -1;
				mmptr[value].lcount = 0;
				mmptr[value].rcount = 0;
				mmptr[value].lupval = lupval;
				mmptr[value].rupval = rupval;
				if (left != -1) { mmptr[left].right = value; }
				if (right != -1) { mmptr[right].left = value; }
				break;
			} else {
				curr = mmptr[curr].rchild;
			}
		} else {
			right = curr;
			rupval = mmptr[curr].value;
			mmptr[curr].lcount += 1;
			if (mmptr[curr].lchild == -1) {
				mmptr[curr].lchild = value;
				mmptr[value].parent = curr;
				mmptr[value].left = left;
				mmptr[value].right = right;
				mmptr[value].lchild = -1;
				mmptr[value].rchild = -1;
				mmptr[value].lcount = 0;
				mmptr[value].rcount = 0;
				mmptr[value].lupval = lupval;
				mmptr[value].rupval = rupval;
				if (left != -1) { mmptr[left].right = value; }
				if (right != -1) { mmptr[right].left = value; }
				break;
			} else {
				curr = mmptr[curr].lchild;
			}
		}
	}
	return root;
}

index_t rebalance(Block *mmptr, index_t root, uint64_t threshold) {
	if ((root == -1) || (mmptr[root].lcount + mmptr[root].rcount < 2)) { return root; }
	while (mmptr[root].rcount > mmptr[root].lcount + 1) {
		if (mmptr[root].rchild == mmptr[root].right) {
			mmptr[mmptr[root].right].lupval = mmptr[root].lupval;
			mmptr[mmptr[root].right].rupval = mmptr[root].rupval;
			mmptr[root].rupval = mmptr[mmptr[root].right].value;
			mmptr[root].parent = mmptr[root].right;
			mmptr[mmptr[root].parent].parent = -1;
			mmptr[mmptr[root].parent].lchild = root;
			mmptr[root].rchild = -1;
			mmptr[mmptr[root].parent].lcount = mmptr[root].lcount + 1;
			mmptr[root].rcount = 0;
			root = mmptr[root].parent;
		} else {
			index_t old_l = mmptr[root].lchild;
			mmptr[root].rcount -= 1;
			index_t curr = mmptr[root].rchild;
			while ((curr != -1) && (mmptr[curr].lchild != -1)) {
				mmptr[curr].lcount -= 1;
				mmptr[curr].lupval = mmptr[mmptr[root].right].value;
				curr = mmptr[curr].lchild;
			}
			mmptr[mmptr[mmptr[root].right].parent].lchild = mmptr[mmptr[root].right].rchild;
			if (mmptr[mmptr[root].right].rchild != -1) { mmptr[mmptr[mmptr[root].right].rchild].parent = mmptr[mmptr[root].right].parent; }
			mmptr[mmptr[root].right].parent = -1;
			mmptr[mmptr[root].right].lchild = mmptr[root].lchild;
			mmptr[mmptr[root].right].rchild = mmptr[root].rchild;
			mmptr[mmptr[root].right].lcount = mmptr[root].lcount;
			mmptr[mmptr[root].right].rcount = mmptr[root].rcount;
			mmptr[mmptr[root].right].lupval = mmptr[root].lupval;
			mmptr[mmptr[root].right].rupval = mmptr[root].rupval;
			if (mmptr[mmptr[root].right].lchild != -1) { mmptr[mmptr[mmptr[root].right].lchild].parent = mmptr[root].right; }
			if (mmptr[mmptr[root].right].rchild != -1) { mmptr[mmptr[mmptr[root].right].rchild].parent = mmptr[root].right; }
			mmptr[root].lchild = -1;
			mmptr[root].rchild = -1;
			mmptr[root].lcount = 0;
			mmptr[root].rcount = 0;
			mmptr[root].rupval = mmptr[mmptr[root].right].value;
			if (old_l != -1) {
				curr = old_l;
				while (mmptr[curr].rchild) {
					mmptr[curr].rcount += 1;
					mmptr[curr].rupval = mmptr[mmptr[root].right].value;
					curr = mmptr[curr].rchild;
				}
				mmptr[mmptr[root].left].rupval = mmptr[mmptr[root].right].value;
				mmptr[root].parent = mmptr[root].left;
				mmptr[mmptr[root].left].rchild = root;
				mmptr[mmptr[root].left].rcount = 1;
				mmptr[mmptr[root].right].lcount += 1;
				mmptr[root].lupval = mmptr[mmptr[root].left].value;
			} else {
				mmptr[root].parent = mmptr[root].right;
				mmptr[mmptr[root].right].lchild = root;
				mmptr[mmptr[root].right].lcount = 1;
				mmptr[root].lupval = mmptr[mmptr[root].right].lupval;
			}
			root = mmptr[root].right;
		}
	}
	while (mmptr[root].lcount > mmptr[root].rcount + 1) {
		if (mmptr[root].lchild == mmptr[root].left) {
			mmptr[mmptr[root].left].lupval = mmptr[root].lupval;
			mmptr[mmptr[root].left].rupval = mmptr[root].rupval;
			mmptr[root].lupval = mmptr[mmptr[root].left].value;
			mmptr[root].parent = mmptr[root].left;
			mmptr[mmptr[root].parent].parent = -1;
			mmptr[mmptr[root].parent].rchild = root;
			mmptr[root].lchild = -1;
			mmptr[mmptr[root].parent].rcount = mmptr[root].rcount + 1;
			mmptr[root].lcount = 0;
			root = mmptr[root].parent;
		} else {
			index_t old_r = mmptr[root].rchild;
			mmptr[root].lcount -= 1;
			index_t curr = mmptr[root].lchild;
			while ((curr != -1) && (mmptr[curr].rchild != -1)) {
				mmptr[curr].rcount -= 1;
				mmptr[curr].rupval = mmptr[mmptr[root].left].value;
				curr = mmptr[curr].rchild;
			}
			mmptr[mmptr[mmptr[root].left].parent].rchild = mmptr[mmptr[root].left].lchild;
			if (mmptr[mmptr[root].left].lchild != -1) { mmptr[mmptr[mmptr[root].left].lchild].parent = mmptr[mmptr[root].left].parent; }
			mmptr[mmptr[root].left].parent = -1;
			mmptr[mmptr[root].left].lchild = mmptr[root].lchild;
			mmptr[mmptr[root].left].rchild = mmptr[root].rchild;
			mmptr[mmptr[root].left].lcount = mmptr[root].lcount;
			mmptr[mmptr[root].left].rcount = mmptr[root].rcount;
			mmptr[mmptr[root].left].lupval = mmptr[root].lupval;
			mmptr[mmptr[root].left].rupval = mmptr[root].rupval;
			if (mmptr[mmptr[root].left].lchild != -1) { mmptr[mmptr[mmptr[root].left].lchild].parent = mmptr[root].left; }
			if (mmptr[mmptr[root].left].rchild != -1) { mmptr[mmptr[mmptr[root].left].rchild].parent = mmptr[root].right; }
			mmptr[root].lchild = -1;
			mmptr[root].rchild = -1;
			mmptr[root].lcount = 0;
			mmptr[root].rcount = 0;
			mmptr[root].lupval = mmptr[mmptr[root].left].value;
			if (old_r != -1) {
				curr = old_r;
				while (mmptr[curr].lchild != -1) {
					mmptr[curr].lcount += 1;
					mmptr[curr].lupval = mmptr[mmptr[root].left].value;
					curr = mmptr[curr].lchild;
				}
				mmptr[mmptr[root].right].lupval = mmptr[mmptr[root].left].value;
				mmptr[root].parent = mmptr[root].right;
				mmptr[mmptr[root].right].lchild = root;
				mmptr[mmptr[root].right].lcount = 1;
				mmptr[mmptr[root].left].rcount += 1;
				mmptr[root].rupval = mmptr[mmptr[root].right].value;
			} else {
				mmptr[root].parent = mmptr[root].left;
				mmptr[mmptr[root].left].rchild = root;
				mmptr[mmptr[root].left].rcount = 1;
				mmptr[root].rupval = mmptr[mmptr[root].left].rupval;
			}
			root = mmptr[root].left;
		}
	}
	if ((mmptr[root].lchild != -1) && (mmptr[mmptr[root].lchild].lcount + mmptr[mmptr[root].lchild].rcount >= threshold)) {
		mmptr[root].lchild = rebalance(mmptr, mmptr[root].lchild, threshold);
		mmptr[mmptr[root].lchild].parent = root;
	}
	if ((mmptr[root].rchild != -1) && (mmptr[mmptr[root].rchild].lcount + mmptr[mmptr[root].rchild].rcount >= threshold)) {
		mmptr[root].rchild = rebalance(mmptr, mmptr[root].rchild, threshold);
		mmptr[mmptr[root].rchild].parent = root;
	}
	return root;
}

static int format(int fd) {
	char buffer[64];
	printf("\nEnter region size [64MiB]: ");
	while (fgets(buffer, 63, stdin) == NULL) { printf("\nEnter region size [64MiB]: "); }
	uint64_t region_size = 0;
	char i = 0;
	while (('0' <= buffer[i]) && (buffer[i] <= '9')) { region_size = region_size * 10 + buffer[i] - 0x30; ++i; }
	if ((buffer[i] != 0x00) && (buffer[i] != 0x10)) {
		if ((buffer[i + 1] | ('a' ^ 'A')) == 'i') {
			if ((buffer[i] | ('a' ^ 'A')) == 'k') { region_size <<= 10; }
			if ((buffer[i] | ('a' ^ 'A')) == 'm') { region_size <<= 20; }
			if ((buffer[i] | ('a' ^ 'A')) == 'g') { region_size <<= 30; }
			if ((buffer[i] | ('a' ^ 'A')) == 't') { region_size <<= 40; }
		} else {
			if ((buffer[i] | ('a' ^ 'A')) == 'k') { region_size *= 1000; }
			if ((buffer[i] | ('a' ^ 'A')) == 'm') { region_size *= 1000000; }
			if ((buffer[i] | ('a' ^ 'A')) == 'g') { region_size *= 1000000000; }
			if ((buffer[i] | ('a' ^ 'A')) == 't') { region_size *= 1000000000000; }
		}
	}
	if (region_size == 0) { region_size = 1 << 26; }
	if ((region_size & 127) != 0) {
		fprintf(stderr, "region size must be a positive multiple of 128 bytes\n");
		return -1;
	}
	uint64_t total_size;
	struct stat st;
	if (fstat(fd, &st) < 0) { return -1; }
	if (S_ISBLK(st.st_mode)) {
		if (ioctl(fd, BLKGETSIZE64, &total_size) < 0) { return -1; }
	} else if (S_ISREG(st.st_mode)) {
		off_t end = lseek(fd, 0, SEEK_END);
		if (end < 0) { return -1; }
		total_size = (uint64_t)end;
	} else { errno = ENOTSUP; return -1; }
	
	uint64_t region_blocks = region_size >> 7;
	uint64_t region_count = total_size / region_size;
	if (region_count == 0) {
		fprintf(stderr, "device is smaller than one region (%llu bytes)\n", region_size);
		return -1;
	}
	
	for (uint64_t i = 0; i < region_count; ++i) {
		uint64_t region_offset = i * region_size;
		
		Metadata mdata;
		memset(&mdata, 0, sizeof(mdata));
		mdata.sig1 = SIGNATURE_1;
		mdata.sig2 = SIGNATURE_2;
		mdata.root = 1;
		mdata.region_count = region_count;
		mdata.region_size = region_blocks;
		mdata.region_index = i;
		mdata.region_type = ((i == 0)? REGION_TYPE_NODES : REGION_TYPE_NULL);
		mdata.size_filled = ((i == 0)? 1 : 0);
		mdata.size_exfilled = 0;
		if (pwrite(fd, &mdata, 128, region_offset) != 128) { return -1; }
	}
	Node root_node;
	makeroot(&root_node);
	if (pwrite(fd, &root_node, 128, 128) != 128) { return -1; }
	
	if (fsync(fd) < 0) {
		fprintf(stderr, "fsync: %s\n", strerror(errno));
		return -1;
	}
	
	printf("Formatted: %llu regions of %llu bytes (%llu blocks each), %llu bytes discarded\n", region_count, region_size, region_blocks, total_size - region_count * region_size);
	return 0;
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		if (!(strcmp(argv[i], "--help") && strcmp(argv[i], "-h"))) {
			printf("Usage:\ntreedsk <device name>\ntreedsk --help|-h\n");
			return 0;
		}
	}
	if (argc != 2) {
		printf("Usage:\ntreedsk <device name>\ntreedsk --help|-h\n");
		return 0;
	}
	
	int fd = open(argv[1], O_RDWR);
	if (fd < 0) { perror("open"); return 1; }
	char mdatbuf[128];
	ssize_t n = pread(fd, mdatbuf, 128, 0);
	if (n < 0) { fprintf(stderr, "pread: %s\n", strerror(errno)); return 2; }
	if (n != 128) { fprintf(stderr, "pread: small device\n"); return 3; }
	Metadata mdata;
	memcpy(&mdata, mdatbuf, sizeof(mdata));
	if ((mdata.sig1 != SIGNATURE_1) || (mdata.sig2 != SIGNATURE_2)) {
		char buffer[64];
		printf("\nDevice unformatted: signature (region 0) broken\nFormat device? [y/N]: ");
		if (fgets(buffer, 64, stdin) != NULL) {
			if ((buffer[0] | ('a' ^ 'A')) != 'y') { return 0; }
			if (format(fd)) { return 4; }
		}
	}
	
	printf("\n\nDevice (?already?) formatted: %llu regions of %llu bytes, root at offset %llu\n", mdata.region_count, mdata.region_size * 128, mdata.root);
	
	Block *mmptr = (Block *)mmap(0, mdata.region_count * mdata.region_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!mmptr) { return 5; }
	if (mdata.region_size <= (1 << 21)) {
		ssize_t n;
		uint64_t i = 0;
		while (1) {
			if (mprotect((void *)(((uint64_t)mmptr) + i * mdata.region_size * 128), mdata.region_size * 128, PROT_READ | PROT_WRITE)) { return 6; }
			n = pread(fd, (void *)(((uint64_t)mmptr) + (i * mdata.region_size * 128)), mdata.region_size * 128, i * mdata.region_size * 128);
			if (n != mdata.region_size * 128) { fprintf(stderr, "pread: general reads\n"); return 7; }
			if ((i == mdata.region_count - 1) || (((Metadata *)(((uint64_t)mmptr) + i * mdata.region_size * 128))->region_type == REGION_TYPE_NULL)) { break; }
		}
	}
	// continue caching mechanism
	return -1;
}
