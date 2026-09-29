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

typedef struct _node Node;

struct _node {
	uint64_t	value;
	Node*		parent;
	Node*		left;
	Node*		right;
	Node*		lchild;
	Node*		rchild;
	uint64_t	lcount;
	uint64_t	rcount;

	uint64_t	lupval;
	uint64_t	rupval;
	uint8_t		is_real;
	uint8_t		pad1[47];
};

Node *makeroot (Node *root) {
	root->parent = (Node *)0;
	root->left = (Node *)0;
	root->right = (Node *)0;
	root->lchild = (Node *)0;
	root->rchild = (Node *)0;
	root->lcount = 0;
	root->rcount = 0;
	root->lupval = 0;
	root->rupval = 0xffffffffffffffffULL;
	root->is_real = 0;
	return root;
}

Node *search (Node *root, uint64_t value) {
	Node *curr = root;
	while (1) {
		if (value > curr->value) {
			if (!(curr->rchild)) { return (Node *)0; }
			curr = curr->rchild;
		else if (value < curr->value) {
			if (!(curr->lchild)) { return (Node *)0; }
			curr = curr->lchild;
		else { return curr; }
	}
	return (Node *)0;
}

Node *insert (Node *root, Node *value) {
	Node *curr = root;
	Node *left = (Node *)0;
	Node *right = (Node *)0;
	uint64_t lupval = root->lupval;
	uint64_t rupval = root->rupval;
	while (1) {
		if (value->value > curr->value) {
			left = curr;
			lupval = curr->value;
			curr->rcount += 1;
			if (curr->rchild == (Node *)0) {
				curr->rchild = value;
				value->parent = curr;
				value->left = left;
				value->right = right;
				value->lchild = (Node *)0;
				value->rchild = (Node *)0;
				value->lcount = 0;
				value->rcount = 0;
				value->lupval = lupval;
				value->rupval = rupval;
				if (left != (Node *)0) { left->right = value; }
				if (right != (Node *)0) { right->left = value; }
				break;
			} else {
				curr = curr->rchild;
			}
		} else {
			right = curr;
			rupval = curr->value;
			curr->lcount += 1;
			if (curr->lchild == (Node *)0) {
				curr->lchild = value;
				value->parent = curr;
				value->left = left;
				value->right = right;
				value->lchild = (Node *)0;
				value->rchild = (Node *)0;
				value->lcount = 0;
				value->rcount = 0;
				value->lupval = lupval;
				value->rupval = rupval;
				if (left != (Node *)0) { left->right = value; }
				if (right != (Node *)0) { right->left = value; }
				break;
			} else {
				curr = curr->lchild;
			}
		}
	}
	return root;
}

Node *rebalance(Node *root, uint64_t threshold) {
	if ((!root) || (root->lcount + root->rcount < 2)) { return root; }
	while (root->rcount > root->lcount + 1) {
		if (root->rchild == root->right) {
			root->right->lupval = root->lupval;
			root->right->rupval = root->rupval;
			root->rupval = root->right->value;
			root->parent = root->right;
			root->parent->parent = (Node *)0;
			root->parent->lchild = root;
			root->rchild = (Node *)0;
			root->parent->lcount = root->lcount + 1;
			root->rcount = 0;
			root = root->parent;
		} else {
			Node *old_l = root->lchild;
			root->rcount -= 1;
			Node *curr = root->rchild;
			while (curr && (curr->lchild)) {
				curr->lcount -= 1;
				curr->lupval = root->right->value;
				curr = curr->lchild;
			}
			root->right->parent->lchild = root->right->rchild;
			if (root->right->rchild) { root->right->rchild->parent = root->right->parent; }
			root->right->parent = (Node *)0;
			root->right->lchild = root->lchild;
			root->right->rchild = root->rchild;
			root->right->lcount = root->lcount;
			root->right->rcount = root->rcount;
			root->right->lupval = root->lupval;
			root->right->rupval = root->rupval;
			if (root->right->lchild) { root->right->lchild->parent = root->right; }
			if (root->right->rchild) { root->right->rchild->parent = root->right; }
			root->lchild = (Node *)0;
			root->rchild = (Node *)0;
			root->lcount = 0;
			root->rcount = 0;
			root->rupval = root->right->value;
			if (old_l) {
				curr = old_l;
				while (curr->rchild) {
					curr->rcount += 1;
					curr->rupval = root->right->value;
					curr = curr->rchild;
				}
				root->left->rupval = root->right->value;
				root->parent = root->left;
				root->left->rchild = root;
				root->left->rcount = 1;
				root->right->lcount += 1;
				root->lupval = root->left->value;
			} else {
				root->parent = root->right;
				root->right->lchild = root;
				root->right->lcount = 1;
				root->lupval = root->right->lupval;
			}
			root = root->right;
		}
	}
	while (root->lcount > root->rcount + 1) {
		if (root->lchild == root->left) {
			root->left->lupval = root->lupval;
			root->left->rupval = root->rupval;
			root->lupval = root->left->value;
			root->parent = root->left;
			root->parent->parent = (Node *)0;
			root->parent->rchild = root;
			root->lchild = (Node *)0;
			root->parent->rcount = root->rcount + 1;
			root->lcount = 0;
			root = root->parent;
		} else {
			Node *old_r = root->rchild;
			root->lcount -= 1;
			Node *curr = root->lchild;
			while (curr && (curr->rchild)) {
				curr->rcount -= 1;
				curr->rupval = root->left->value;
				curr = curr->rchild;
			}
			root->left->parent->rchild = root->left->lchild;
			if (root->left->lchild) { root->left->lchild->parent = root->left->parent; }
			root->left->parent = (Node *)0;
			root->left->lchild = root->lchild;
			root->left->rchild = root->rchild;
			root->left->lcount = root->lcount;
			root->left->rcount = root->rcount;
			root->left->lupval = root->lupval;
			root->left->rupval = root->rupval;
			if (root->left->lchild) { root->left->lchild->parent = root->left; }
			if (root->left->rchild) { root->left->rchild->parent = root->left; }
			root->lchild = (Node *)0;
			root->rchild = (Node *)0;
			root->lcount = 0;
			root->rcount = 0;
			root->lupval = root->left->value;
			if (old_r) {
				curr = old_r;
				while (curr->lchild) {
					curr->lcount += 1;
					curr->lupval = root->left->value;
					curr = curr->lchild;
				}
				root->right->lupval = root->left->value;
				root->parent = root->right;
				root->right->lchild = root;
				root->right->lcount = 1;
				root->left->rcount += 1;
				root->rupval = root->right->value;
			} else {
				root->parent = root->left;
				root->left->rchild = root;
				root->left->rcount = 1;
				root->rupval = root->left->rupval;
			}
			root = root->left;
		}
	}
	if (root->lchild && (root->lchild->lcount + root->lchild->rcount >= threshold)) {
		root->lchild = rebalance(root->lchild, threshold);
		root->lchild->parent = root;
	}
	if (root->rchild && (root->rchild->lcount + root->rchild->rcount >= threshold)) {
		root->rchild = rebalance(root->rchild, threshold);
		root->rchild->parent = root;
	}
	return root;
}

#define SIGNATURE_1	0x6873616865657274ULL
#define SIGNATURE_2	0x656c6966736e6962ULL

#define REGION_TYPE_NULL	0x00
#define REGION_TYPE_NODES	0x01
#define REGION_TYPE_RAWDATA	0x02
#define REGION_TYPE_NODE_EXTEND	0x03

typedef struct _metadata Metadata;

struct _metadata {
	uint64_t	sig1;
	uint64_t	sig2;
	Node*		root;
	uint64_t	region_count;
	uint64_t	region_size;
	uint64_t	region_index;
	uint64_t	size_filled;
	uint8_t		region_type;
	uint8_t		pad1[7];
	uint64_t	pad2[8];
};

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
		mdata.root = (Node *)128;
		mdata.region_count = region_count;
		mdata.region_size = region_blocks;
		mdata.region_index = i;
		mdata.region_type = ((i == 0)? REGION_TYPE_NODES : REGION_TYPE_NULL);
		mdata.size_filled = ((i == 0)? 1 : 0);
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
	
	void *mmptr = mmap(0, mdata.region_count * mdata.region_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!mmptr) { return 0; }
	// add actual cacher
}
