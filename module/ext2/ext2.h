#ifndef EXT2_H
#define EXT2_H

#include <typedefs.h>

struct ext2_super_block {
	uint32_t s_inodes_count;	/* Total number of inodes */
	uint32_t s_blocks_count;	/* Total number of blocks */
	uint32_t s_r_blocks_count;	/* Total number of blocks reserved for the super user */
	uint32_t s_free_blocks_count;	/* Total number of free blocks */
	uint32_t s_free_inodes_count;	/* Total number of free inodes */
	uint32_t s_first_data_block;	/* Id of the block containing the superblock structure */
	uint32_t s_log_block_size;	/* Used to compute block size = 1024 << s_log_block_size */
	uint32_t s_log_frag_size;	/* Used to compute fragment size */
	uint32_t s_blocks_per_group;	/* Total number of blocks per group */
	uint32_t s_frags_per_group;	/* Total number of fragments per group */
	uint32_t s_inodes_per_group;	/* Total number of inodes per group */
	uint32_t s_mtime;		/* Last time the file system was mounted */
	uint32_t s_wtime;		/* Last write access to the file system */
	uint16_t s_mnt_count;	/* How many `mount' since the last was full verification */
	uint16_t s_max_mnt_count;	/* Max count between mount */
	uint16_t s_magic;		/* = 0xEF53 */
	uint16_t s_state;		/* File system state */
	uint16_t s_errors;		/* Behaviour when detecting errors */
	uint16_t s_minor_rev_level;	/* Minor revision level */
	uint32_t s_lastcheck;	/* Last check */
	uint32_t s_checkinterval;	/* Max. time between checks */
	uint32_t s_creator_os;	/* = 5 */
	uint32_t s_rev_level;	/* = 1, Revision level */
	uint16_t s_def_resuid;	/* Default uid for reserved blocks */
	uint16_t s_def_resgid;	/* Default gid for reserved blocks */
	uint32_t s_first_inwq;
	/* First inode useable for standard files */
	uint16_t s_inode_size;	/* Inode size */
	uint16_t s_block_group_nr;	/* Block group hosting this superblock structure */
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint8_t s_uuid[16];		/* Volume id */
	char s_volume_name[16];	/* Volume name */
	char s_last_mounted[64];	/* Path where the file system was last mounted */
	uint32_t s_algo_bitmap;	/* For compression */
	uint8_t s_padding[820];
} __attribute__ ((packed));

struct ext2_group_desc {
	uint32_t bg_block_bitmap;	/* Id of the first block of the "block bitmap" */
	uint32_t bg_inode_bitmap;	/* Id of the first block of the "inode bitmap" */
	uint32_t bg_inode_table;	/* Id of the first block of the "inode table" */
	uint16_t bg_free_blocks_count;	/* Total number of free blocks */
	uint16_t bg_free_inodes_count;	/* Total number of free inodes */
	uint16_t bg_used_dirs_count;	/* Number of inodes allocated to directories */
	uint16_t bg_pad;		/* Padding the structure on a 32bit boundary */
	uint32_t bg_reserved[3];	/* Future implementation */
} __attribute__ ((packed));

struct ext2_inode {
	uint16_t i_mode;		/* File type + access rights */
	uint16_t i_uid;
	uint32_t i_size;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;
	uint32_t i_dtime;
	uint16_t i_gid;
	uint16_t i_links_count;
	uint32_t i_blocks;		/* 512 bytes blocks ! */
	uint32_t i_flags;
	uint32_t i_osd1;

	/*
	 * [0] -> [11] : block number (32 bits per block)
	 * [12]        : indirect block number
	 * [13]        : bi-indirect block number
	 * [14]        : tri-indirect block number
	 */
	uint32_t i_block[15];

	uint32_t i_generation;
	uint32_t i_file_acl;
	uint32_t i_dir_acl;
	uint32_t i_faddr;
	uint8_t i_osd2[12];
} __attribute__ ((packed));

struct ext2_directory_entry {
	uint32_t inode;		/* inode number or 0 (unused) */
	uint16_t rec_len;		/* offset to the next dir. entry */
	uint8_t name_len;		/* name length */
	uint8_t file_type;
	char name;
} __attribute__ ((packed));

struct ext2_disk {
	struct ext2_super_block*	sb;
	struct ext2_group_desc*	gd;
	uint32_t 				blocksize;
	uint32_t 				groups;		/* Total number of groups */
	uint32_t 				inodes_per_group;
	vfs_node_t*				dev;
};

#define	EXT2_ERRORS_CONTINUE	1
#define	EXT2_ERRORS_RO		2
#define	EXT2_ERRORS_PANIC	3
#define	EXT2_ERRORS_DEFAULT	1

/* inode: i_mode */
#define	EXT2_S_IFMT	0xF000		/* format mask  */
#define	EXT2_S_IFSOCK	0xC000	/* socket */
#define	EXT2_S_IFLNK	0xA000	/* symbolic link */
#define	EXT2_S_IFREG	0x8000	/* regular file */
#define	EXT2_S_IFBLK	0x6000	/* block device */
#define	EXT2_S_IFDIR	0x4000	/* directory */
#define	EXT2_S_IFCHR	0x2000	/* character device */
#define	EXT2_S_IFIFO	0x1000	/* fifo */

#define	EXT2_S_ISUID	0x0800	/* SUID */
#define	EXT2_S_ISGID	0x0400	/* SGID */
#define	EXT2_S_ISVTX	0x0200	/* sticky bit */
#define	EXT2_S_IRWXU	0x01C0	/* user access rights mask */
#define	EXT2_S_IRUSR	0x0100	/* read */
#define	EXT2_S_IWUSR	0x0080	/* write */
#define	EXT2_S_IXUSR	0x0040	/* execute */
#define	EXT2_S_IRWXG	0x0038	/* group access rights mask */
#define	EXT2_S_IRGRP	0x0020	/* read */
#define	EXT2_S_IWGRP	0x0010	/* write */
#define	EXT2_S_IXGRP	0x0008	/* execute */
#define	EXT2_S_IRWXO	0x0007	/* others access rights mask */
#define	EXT2_S_IROTH	0x0004	/* read */
#define	EXT2_S_IWOTH	0x0002	/* write */
#define	EXT2_S_IXOTH	0x0001	/* execute */

#endif
