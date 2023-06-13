/*
 * SVSFS - a close copy of D. Thain's SimpleFS.
 * PJF 4/2023
 * original comments below
 * Implementation of SimpleFS.
 * Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

extern struct disk *thedisk;

#define FS_MAGIC           0x34341023
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 3
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	uint32_t magic;
	uint32_t nblocks;
	uint32_t ninodeblocks;
	uint32_t ninodes;
};

struct fs_inode {
	uint32_t isvalid;
	uint32_t size;
	int64_t ctime;
	uint32_t direct[POINTERS_PER_INODE];
	uint32_t indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	unsigned char data[BLOCK_SIZE];
};


struct fs_superblock super_block = {0};
int* free_blocks = NULL;
int mounted = 0;

int fs_format() {
    // already mounted
	if (mounted) {
		printf("already mounted.\n");
		return 0;
	}

	// read one block of data from the disk
	union fs_block block;

	// create a superblock
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_nblocks(thedisk);
	
	// set aside 10% of blocks for inodes
	block.super.ninodeblocks = block.super.nblocks / 10 + (block.super.nblocks % 10 == 0 ? 0 : 1);
	block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;

	// write the superblock value on disk
	disk_write(thedisk, 0, block.data);
    super_block = block.super;

	// initialize inode table
    unsigned char data[BLOCK_SIZE] = {0};
    for (int block_num; block_num < block.super.nblocks; block_num++) {
        disk_write(thedisk, block_num, data);
    }
	return 1;
}

void fs_debug() {
	union fs_block block;

	disk_read(thedisk,0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

    // get inode block
    for (uint32_t iblock = 1; iblock <= block.super.ninodeblocks; iblock++) {
        union fs_block inode_block = {{0}};
        disk_read(thedisk, iblock, inode_block.data);
        // get inode
        for (int inumber = 0; inumber < INODES_PER_BLOCK; inumber++) {
            if (!inode_block.inode[inumber].isvalid) 
                continue;
            printf("inode %d:\n", (iblock - 1) * INODES_PER_BLOCK + inumber);
			printf("    size: %d bytes\n", inode_block.inode[inumber].size);
			printf("    created: %s",  ctime(&inode_block.inode[inumber].ctime));
            // get direct block
			printf("    direct block: ");
            for (int direct_pointer = 0; direct_pointer < POINTERS_PER_INODE; direct_pointer++) {
                if (inode_block.inode[inumber].direct[direct_pointer] == 0)
                    continue;
                printf(" %d", inode_block.inode[inumber].direct[direct_pointer]);
            }
            printf("\n");
            if (inode_block.inode[inumber].indirect == 0)
                continue;
            // get indirect block
            printf("    indirect block: %d\n", inode_block.inode[inumber].indirect);

            // get indirect block data
            printf("    indirect data blocks:");
            union fs_block indirect_block = {{0}};
            disk_read(thedisk, inode_block.inode[inumber].indirect, indirect_block.data);
            for (int indirect_pointer = 0; indirect_pointer < POINTERS_PER_BLOCK; indirect_pointer++) {
                if (indirect_block.pointers[indirect_pointer] == 0)
                    continue;
                printf(" %d", indirect_block.pointers[indirect_pointer]);
            }
            printf("\n");
        }
    }
}

int fs_mount() {
    // already mounted
	if (mounted) {
		printf("already mounted.\n");
		return 0;
	}

    union fs_block block;
	disk_read(thedisk, 0, block.data);

    if (block.super.magic != FS_MAGIC || block.super.ninodeblocks * INODES_PER_BLOCK > block.super.ninodes || block.super.ninodeblocks < block.super.nblocks / 10)
        return 0;
    super_block = block.super;

    // inititial free_blocks bitmap
    free_blocks = calloc(super_block.nblocks, sizeof(int));
    for (int block_num = 1; block_num < super_block.nblocks; block_num++) 
        free_blocks[block_num] = 1;
    
    for (int iblock = 1; iblock <= super_block.ninodeblocks; iblock++) {
        union fs_block inode_block;
        disk_read(thedisk, iblock, inode_block.data);
        free_blocks[iblock] = 0;
        for (int inumber = 0; inumber < INODES_PER_BLOCK; inumber++) {
            if (inode_block.inode[inumber].isvalid) {
                for (int direct_pointer = 0; direct_pointer < POINTERS_PER_INODE; direct_pointer++) {
                    if (inode_block.inode[inumber].direct[direct_pointer] == 0)
                        continue;
                    free_blocks[inode_block.inode[inumber].direct[direct_pointer]] = 0;
                    if (inode_block.inode[inumber].indirect == 0)
                        continue;
                    free_blocks[inode_block.inode[inumber].indirect] = 0;
                    union fs_block indirect_block;
                    disk_read(thedisk, inode_block.inode[inumber].indirect, indirect_block.data);
                    for (int indirect_pointer = 0; indirect_pointer < POINTERS_PER_BLOCK; indirect_pointer++) {
                        if (indirect_block.pointers[indirect_pointer] == 0)
                            continue;
                        free_blocks[indirect_block.pointers[indirect_pointer]] = 0;
                    }
                }
            }
        }
    }
    /*for (int i = 0; i < super_block.nblocks; i++) {
        printf("block = %d, free = %d\n", i, free_blocks[i]);
    }*/

    mounted = 1;
	return 1;
}

int fs_create() {
    // not mounted
	if (!mounted) {
		printf("not mounted yet.\n");
		return 0;
    }
    union fs_block inode_block = {{0}};
    // finding first invalid inode in inode blocks
    for (int iblock = 0; iblock < super_block.ninodeblocks; iblock++) {
        disk_read(thedisk, iblock + 1, inode_block.data);
        for (int inumber = 0; inumber < INODES_PER_BLOCK; inumber++) {
            if (iblock == 0 && inumber == 0)
                continue;
            if (!inode_block.inode[inumber].isvalid) {
                inode_block.inode[inumber].isvalid = 1;
                inode_block.inode[inumber].size = 0;
                inode_block.inode[inumber].ctime = time(NULL);
                for (int pointer = 0; pointer < POINTERS_PER_INODE; pointer++)
                    inode_block.inode[inumber].direct[pointer] = 0;
                // set indirect pointer
                inode_block.inode[inumber].indirect = 0;
                disk_write(thedisk, iblock + 1, inode_block.data);
                return iblock * INODES_PER_BLOCK + inumber;
            }
        }
    }
    printf("no empty inode\n");
	return 0;
}

int verify_inumber(int inumber) {
	// check for valid inumber
	if (inumber <= 0 || inumber >= super_block.ninodes)
		return 0;
	return 1;
}

int fs_delete( int inumber ){
    // not mounted
	if (!mounted) {
		printf("not mounted yet.\n");
		return 0;
	}

    if (verify_inumber(inumber) == 0) {
		printf("invalid inode number: %d\n", inumber);
		return 0;
	}

    // load inode
	int block_num = inumber / INODES_PER_BLOCK + 1;
    int inode_index = inumber % INODES_PER_BLOCK;
    union fs_block block;
    disk_read(thedisk, block_num, block.data);

    // check if inode is valid
    if (!block.inode[inode_index].isvalid) {
        printf("inode %d is invalid.\n", inumber);
        return 0;
    }

    // release all direct block
    for (int direct_pointer = 0; direct_pointer < POINTERS_PER_INODE; direct_pointer++) {
        if (block.inode[inode_index].direct[direct_pointer] == 0)
            continue;
        free_blocks[block.inode[inode_index].direct[direct_pointer]] = 1;
        block.inode[inode_index].direct[direct_pointer] = 0;
    }

    // release all indirect block
    if (block.inode[inode_index].indirect != 0) {
        union fs_block indirect_block;
        disk_read(thedisk, block.inode[inode_index].indirect, indirect_block.data);
        for (int indirect_pointer = 0; indirect_pointer < POINTERS_PER_BLOCK; indirect_pointer++) {
            if (indirect_block.pointers[indirect_pointer] == 0)
                continue;
            free_blocks[indirect_block.pointers[indirect_pointer]] = 1;
            indirect_block.pointers[indirect_pointer] = 0;
        }
        free_blocks[block.inode[inode_index].indirect] = 1;
        disk_write(thedisk, block.inode[inode_index].indirect, indirect_block.data);
        block.inode[inode_index].indirect = 0;
    }

    block.inode[inode_index].size = 0;
    block.inode[inode_index].isvalid = 0;
    disk_write(thedisk, block_num, block.data);

    return 1;
}

int fs_getsize( int inumber ) {
    // not mounted
	if (!mounted) {
		printf("not mounted yet.\n");
		return -1;
	}

    if (verify_inumber(inumber) == 0) {
		printf("invalid inode number: %d\n", inumber);
		return 0;
	}

    // load inode
	int block_num = inumber / INODES_PER_BLOCK + 1;
    int inode_index = inumber % INODES_PER_BLOCK;
    union fs_block block;
    disk_read(thedisk, block_num, block.data);

    // check if inode is valid
    if (!block.inode[inode_index].isvalid) {
        printf("inode %d is invalid.\n", inumber);
        return -1;
    }

	return block.inode[inode_index].size;
}

int fs_read( int inumber, unsigned char *data, int length, int offset ) {
	if (!mounted) {
		printf("not mounted yet.\n");
		return 0;
	}

	if (verify_inumber(inumber) == 0) {
		printf("invalid inode number: %d\n", inumber);
		return 0;
	}

    union fs_block block;

    // read block
    int block_num = inumber / INODES_PER_BLOCK + 1;
    int inode_index = inumber % INODES_PER_BLOCK;
    disk_read(thedisk, block_num, block.data);

    // check for valid inode
    if (!block.inode[inode_index].isvalid) {
		printf("inode %d is invalid.\n", inumber);
		return 0;
	}
	
	if(offset > block.inode[inode_index].size) {
		printf("offset is too big.\n");
		return 0;
	}

    // adjust length to account for offset
    if (block.inode[inode_index].size < length + offset)
        length = block.inode[inode_index].size - offset;

    int block_index = offset / BLOCK_SIZE;
    int data_offset = offset % BLOCK_SIZE;
    int nread = 0;
    int ncopy = (length < BLOCK_SIZE) ? length : (BLOCK_SIZE - data_offset);

    while (nread < length) {
        union fs_block data_block = {{0}};
        union fs_block indirect_block = {{0}};

        // read from direct pointer
        if (block_index < POINTERS_PER_INODE)
            disk_read(thedisk, block.inode[inode_index].direct[block_index], data_block.data);
        else {
        // read from indirect pointer
            int indirect_offset = block_index - POINTERS_PER_INODE;
            if (block.inode[inode_index].indirect < super_block.nblocks)
                disk_read(thedisk, block.inode[inode_index].indirect, indirect_block.data);
            if (indirect_block.pointers[indirect_offset] < super_block.nblocks)
                disk_read(thedisk, indirect_block.pointers[indirect_offset], data_block.data);
        }
        memcpy(data + nread, data_block.data + data_offset, ncopy);
        data_offset = 0;
        block_index++;
        nread += ncopy;
        ncopy = ((length - nread) > BLOCK_SIZE) ? BLOCK_SIZE : (length - nread);
    }
    return nread;
}

int find_free_block() {
    for (int block = 1; block < super_block.nblocks; block++) {
        if (free_blocks[block]) {
            free_blocks[block] = 0;
            return block;
        }
    }
    return 0;
}

int get_num_free_block() {
    int count = 0;
    for (int block = 1; block < super_block.nblocks; block++) {
        if (free_blocks[block])
            count++;
    }
    return count;
}

int fs_write( int inumber, const unsigned char *data, int length, int offset ) {
    if (!mounted) {
		printf("not mounted yet.\n");
		return -1;;
	}
    
	if (verify_inumber(inumber) == 0) {
		printf("invalid inode number: %d\n", inumber);
		return -1;;
	}

    union fs_block block;
    // read block
    int block_num = inumber / INODES_PER_BLOCK + 1;
    int inode_index = inumber % INODES_PER_BLOCK;
    disk_read(thedisk, block_num, block.data);
  
    // check for valid inumber
    if (!block.inode[inode_index].isvalid) {
		printf("inode %d is invalid.\n", inumber);
		return -1;
	}

    int nfree = get_num_free_block();
    if (length > nfree * BLOCK_SIZE) {
        printf("there are not enough free block left.\n");
        return 0;
    }

    int block_index = offset / BLOCK_SIZE;
    int data_offset = offset % BLOCK_SIZE;
    int nwrite = 0;
    int ncopy = (length < BLOCK_SIZE) ? length : (BLOCK_SIZE - data_offset);
    
    while (nwrite < length) {
        union fs_block data_block = {{0}};
        union fs_block indirect_block = {{0}};
        int allocated_block = 0;
        if (block_index < POINTERS_PER_INODE) {
            // allocate new block from direct pointer
            if (block.inode[inode_index].direct[block_index] == 0) {
                if ((allocated_block = find_free_block()) == 0) {
					printf("there was no free block found.\n");
					return 0;
				}
                block.inode[inode_index].direct[block_index] = allocated_block;
            }

            // read direct pointers to data block
            if (block.inode[inode_index].direct[block_index] < super_block.nblocks) {
                disk_read(thedisk, block.inode[inode_index].direct[block_index], data_block.data);
                memcpy(data_block.data + data_offset, data + nwrite, ncopy);
                disk_write(thedisk, block.inode[inode_index].direct[block_index], data_block.data);
            }
            else return -1;
        }
        else {
            int indirect_offset = block_index - POINTERS_PER_INODE;
            if (indirect_offset >= POINTERS_PER_BLOCK) {
                printf("all pointers per block are used.\n");
                return nwrite;
            }
            // allocate new block from indirect pointer
            if (block.inode[inode_index].indirect == 0) {
                if ((allocated_block = find_free_block()) == 0) {
					printf("there was no free block found.\n");
                    return 0;
				}
                block.inode[inode_index].indirect = allocated_block;
            }
            else {
                if (block.inode[inode_index].indirect < super_block.nblocks)
                    disk_read(thedisk, block.inode[inode_index].indirect, indirect_block.data);
                else return 0;
			}

            // allocate new block from direct pointers in indirect pointer
            if (indirect_block.pointers[indirect_offset] == 0) {
                if ((allocated_block = find_free_block()) == 0) {
					printf("there was no free block found.\n");
                    return 0;
				}
                if (allocated_block < super_block.nblocks) {
                    indirect_block.pointers[indirect_offset] = allocated_block;
                    disk_write(thedisk, block.inode[inode_index].indirect, indirect_block.data);
                }
            }
            else {
                if (indirect_block.pointers[indirect_offset] < super_block.nblocks)
                    disk_read(thedisk, indirect_block.pointers[indirect_offset], data_block.data);
			}
            
            // copy data into data block
            if (indirect_block.pointers[indirect_offset] < super_block.nblocks) {
                memcpy(data_block.data + data_offset, data + nwrite, ncopy);
                disk_write(thedisk, indirect_block.pointers[indirect_offset], data_block.data);
            }
        }
        if (data_offset + ncopy < BLOCK_SIZE && allocated_block > 0)
            free_blocks[allocated_block] = 1;
        data_offset = 0;
        block_index++;
        nwrite += ncopy;
        ncopy = ((length - nwrite) > BLOCK_SIZE) ? BLOCK_SIZE : (length - nwrite);
        block.inode[inode_index].size = offset + nwrite;
        if (block_num < super_block.nblocks)
            disk_write(thedisk, block_num, block.data);
    }
    return nwrite;
}
