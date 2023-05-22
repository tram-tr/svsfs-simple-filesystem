# Project 6: SVSFS

This is Project 6 of [CSE-34341-SVS-Spring-2023](https://github.com/patrick-flynn/CSE34341-SVS-Sp2023/blob/main/index.md)

## Student

* Tram Trinh (htrinh@nd.edu)

# Project Goals

The goals of this project are:

- Learn about the data structures and implementation of a Unix-like filesystem.
- Learn about filesystem recovery by implementing a free block bitmap scan.
- Develop expertise in C programming by using structures and unions extensively.

# SVSFS Overview

The SVSFS system has three major components: the shell, the filesystem itself, and the emulated disk. The following figure shows how the components relate to each other: 

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/40cda27d-b7c3-4f97-92fd-5426705009e0)

At the top level, a user gives typed commands to a shell, instructing it to format or mount a disk, and to copy data in and out of the filesystem. The shell converts these typed commands into high-level operations on the filesystem, such as fs_format(), fs_mount(), fs_read(), and fs_write(). The filesystem is responsible for accepting these operations on files and converting them into simple block read and write operations on the emulated disk, called disk_read() and disk_write(). The emulated disk, in turn, stores all of its data in an image file in the filesystem.

# SVSFS Filesystem Design

SVSFS has the following layout on disk. It assumes that disk blocks are the common size of 4KB. The first block of the disk is a "superblock" that describes the layout of the rest of the filesystem. A certain number of blocks following the superblock contain inode data structures. Typically, ten percent of the total number of disk blocks are used as inode blocks. The remaining blocks in the filesystem are used as plain data blocks, and occasionally as indirect pointer blocks. Here is a picture of a very small SVSFS image:

![image](https://github.com/tram-tr/svsfs-simple-filesystem/assets/97485876/8e9f70eb-795b-41ae-97f7-eba27cdea43b)






