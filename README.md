# Project 6: SVSFS

This is Project 6 of [CSE-34341-SVS-Spring-2023](https://github.com/patrick-flynn/CSE34341-SVS-Sp2023/blob/main/index.md)

## Student

* Tram Trinh (htrinh@nd.edu)

## Project Goals

The goals of this project are:

- Learn about the data structures and implementation of a Unix-like filesystem.
- Learn about filesystem recovery by implementing a free block bitmap scan.
- Develop expertise in C programming by using structures and unions extensively.

## SVSFS Overview

The SVSFS system has three major components: the shell, the filesystem itself, and the emulated disk. The following figure shows how the components relate to each other: 

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/40cda27d-b7c3-4f97-92fd-5426705009e0)

At the top level, a user gives typed commands to a shell, instructing it to format or mount a disk, and to copy data in and out of the filesystem. The shell converts these typed commands into high-level operations on the filesystem, such as fs_format(), fs_mount(), fs_read(), and fs_write(). The filesystem is responsible for accepting these operations on files and converting them into simple block read and write operations on the emulated disk, called disk_read() and disk_write(). The emulated disk, in turn, stores all of its data in an image file in the filesystem.

## SVSFS Filesystem Design

SVSFS has the following layout on disk. It assumes that disk blocks are the common size of 4KB. The first block of the disk is a "superblock" that describes the layout of the rest of the filesystem. A certain number of blocks following the superblock contain inode data structures. Typically, ten percent of the total number of disk blocks are used as inode blocks. The remaining blocks in the filesystem are used as plain data blocks, and occasionally as indirect pointer blocks. Here is a picture of a very small SVSFS image:

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/8e9f70eb-795b-41ae-97f7-eba27cdea43b)

### The superblock

The superblock contains four 32-bit unsigned integer fields and describes the layout of the rest of the filesystem:

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/3818e4ba-4ef6-4b14-b284-c80eac58f93b)

The first field is always the "magic" number FS_MAGIC (0x34341023). The format routine places this number into the very first bytes of the superblock as a sort of filesystem "signature". When the filesystem is mounted, the OS looks for this magic number. If it is correct, then the disk is assumed to contain a valid filesystem. If some other number is present, then the mount fails, perhaps because the disk is not formatted or contains some other kind of data.


The remaining fields in the superblock describe the layout of the filesystem. nblocks is the total number of blocks, which should be the same as the number of blocks on the disk (thus, a disk may be no larger than 2^32 blocks, or 2^44 bytes – hopefully 16 TB is enough space for us...). ninodeblocks is the number of blocks set aside for storing inodes. ninodes is the total number of inodes in those blocks. The format routine is responsible for choosing ninodeblocks: this should always be 10 percent of nblocks, rounding up. Note that the superblock data structure is quite small: only 16 bytes. The remainder of disk block zero is left unused.

### The inode

Each inode looks like this:

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/a0789840-e9f6-4107-bc79-7f00d830ddfa)

Most fields of the inode are 4-byte (32-bit) integers. The isvalid field is 1 if the inode is valid (i.e. has been created) and is 0 otherwise. The size field contains the logical size of the inode data in bytes. The ctime field is a 64-bit integer indicating the Unix time (man time(2)) that the file was created. There are 3 direct pointers to data blocks, and one pointer to an indirect data block. This design is slightly optimized for files of three blocks or less in size - files longer than this require an additional level of pointer-chasing to locate data blocks numbered 3 and higher. These are not your typical C memory pointers! In this context, "pointer" simply means the disk block number where data may be found. A value of zero (a "null block pointer") indicates that the block pointed to does not exist. Each inode occupies 32 bytes, so there are 128 inodes in each 4KB inode block. Note that an indirect data block is just a big array of pointers to further data blocks. Each pointer is a 32-bit int, and each block is 4KB, so there are 1024 pointers per block. The data blocks are simply 4KB of raw data.


You are probably noticing that something is missing: a free block bitmap! A real filesystem would keep a free block bitmap on disk, recording one bit for each block that was available or in use. This bitmap would be consulted and updated every time the filesystem needed to add or remove a data block from an inode. SVSFS requires the operating system designer (i.e. YOU) to keep a free block bitmap in memory. That is, there must be an array of bits, one for each block of the disk, noting whether the block is in use or available. When it is necessary to allocate a new block for a file, the system must scan through the array to locate an available block. When a block is freed, it must be likewise marked in the bitmap.


SVSFS needs to build the free block bitmap at mount time. Each time that an SimpleFS filesystem is mounted, the system must build a new free block bitmap from scratch by scanning through all of the inodes and recording which blocks are in use. (This is much like performing an fsck every time the system boots.)


SimpleFS looks much like the Unix inode layer. Each "file" is identified by an integer called an "inumber". The inumber is simply an index into the array of inode structures that starts in block 1. When a file is created, SimpleFS chooses the first available inumber and returns it to the user. All further references to that file are made using the inumber. Using SimpleFS as a foundation, you could easily add another layer of software that implements file and directory names. However, that will not be part of this assignment.


## Shell Interface 

To compile:

> <samp>make</samp>

> <samp>./svsfs <\filename>\ <\number-of-blocks>\</samp>
  
For example, to start with a fresh new disk image:
  
> <samp>./svsfs mydisk 200</samp>
  
Once the shell starts, you can use the "help" command to list the available commands:

> <samp>svsfs> help</samp>

> <samp>Commands are:</samp>

> <samp>format</samp>

> <samp>mount</samp> 
  
> <samp>debug</samp> 

> <samp>create</samp> 

> <samp>delete <inode></samp> 

> <samp>cat <inode></samp> 

> <samp>copyin <file> <inode></samp> 

> <samp>copyout <inode> <file></samp> 

> <samp>help</samp> 
  
> <samp>quit</samp> 

> <samp>exit

  
