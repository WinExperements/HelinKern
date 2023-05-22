#include <output.h>
#include <dev.h>
#include <mm/alloc.h>
#include <vfs.h>
#include <mbr.h>
#include <lib/string.h>
#include "fat32.h"

/*
    Based on https://github.com/knusbaum/kernel/blob/master/fat32.c
*/

// this filesystem can be mounted only on ONE drive!
char modname[] __attribute__((section(".modname"))) = "fat32";
static vfs_node_t *disk;
static vfs_fs_t *fat32;

static void trim_spaces(char *c, int max) {
    int i = 0;
    while(*c != ' ' && i++ < max) {
        c++;
    }
    if(*c == ' ') *c = 0;
}

static uint16_t readi16(uint8_t *buff, size_t offset)
{
    uint8_t *ubuff = buff + offset;
    return ubuff[1] << 8 | ubuff[0];
}
static uint32_t readi32(uint8_t *buff, size_t offset) {
    uint8_t *ubuff = buff + offset;
    return
        ((ubuff[3] << 24) & 0xFF000000) |
        ((ubuff[2] << 16) & 0x00FF0000) |
        ((ubuff[1] << 8) & 0x0000FF00) |
        (ubuff[0] & 0x000000FF);
}

// define not implemented or unused functions here
static struct dirent *fat32_readdir(vfs_node_t *node,uint32_t index);

static vfs_node_t *fat32_mount(vfs_node_t *to,void *params) {
	disk = to;
	struct bios_parameter_block *bpb = kmalloc(sizeof(struct bios_parameter_block));
	static uint8_t sector0[512];
	vfs_readBlock(disk,0,512,sector0);
	kprintf("FAT32: Parsing readed data\n");
    bpb->bytes_per_sector = readi16(sector0, 11);;
    bpb->sectors_per_cluster = sector0[13];
    bpb->reserved_sectors = readi16(sector0, 14);
    bpb->FAT_count = sector0[16];
    bpb->dir_entries = readi16(sector0, 17);
    bpb->total_sectors = readi16(sector0, 19);
    bpb->media_descriptor_type = sector0[21];
    bpb->count_sectors_per_FAT12_16 = readi16(sector0, 22);
    bpb->count_sectors_per_track = readi16(sector0, 24);
    bpb->count_heads_or_sizes_on_media = readi16(sector0, 26);
    bpb->count_hidden_sectors = readi32(sector0, 28);
    bpb->large_sectors_on_media = readi32(sector0, 32);
    // EBR
    bpb->count_sectors_per_FAT32 = readi32(sector0, 36);
    bpb->flags = readi16(sector0, 40);
    bpb->FAT_version = readi16(sector0, 42);
    bpb->cluster_number_root_dir = readi32(sector0, 44);
    bpb->sector_number_FSInfo = readi16(sector0, 48);
    bpb->sector_number_backup_boot_sector = readi16(sector0, 50);
    // Skip 12 bytes
    bpb->drive_number = sector0[64];
    bpb->windows_flags = sector0[65];
    bpb->signature = sector0[66];
    bpb->volume_id = readi32(sector0, 67);
    kprintf("FAT32: Copying memory\n");
    memcpy(&bpb->volume_label, sector0 + 71, 11); bpb->volume_label[11] = 0;
    memcpy(&bpb->system_id, sector0 + 82, 8); bpb->system_id[8] = 0;
    kprintf("FAT32: Veryfing FAT32 signature\n");
	trim_spaces(bpb->system_id,8);
	if (!strcmp(bpb->system_id,"FAT32")) {
		kprintf("FAT32: Incorrect SB System ID: %s\n",bpb->system_id);
		kfree(bpb);
		return NULL;
	}
	kprintf("FAT32: Allocation FAT\n");
	uint32_t fatSize = 512 * bpb->count_sectors_per_FAT32;
	int fat_begin_sector = bpb->reserved_sectors;
	int fat_begin_cluster = bpb->FAT_count *  bpb->count_sectors_per_FAT32;
	int fat_cluster_size = 512 * bpb->sectors_per_cluster;
	kprintf("FAT32: Allocation pages\n");
	uint32_t *FAT = kmalloc(fatSize);
	if (!FAT) {
		kprintf("FAT32: Out of memory!\n");
		kfree(bpb);
		return NULL;
	}
	kprintf("FAT32: Reading full FAT\n");
	for (int sector_i = 0; sector_i < bpb->count_sectors_per_FAT32; sector_i++) {
		static uint8_t sector[512];
		vfs_readBlock(disk,(fat_begin_sector + sector_i),512,sector);
		for (int j = 0; j < 512/4; j++) {
			FAT[sector_i * (512/4) + j] = readi32(sector,j * 4);
		}
	}
	kprintf("FAT32: OK\n");
	// Create FAT32 device struct and root inode
	struct fat32_fs *fs = kmalloc(sizeof(struct fat32_fs));
	fs->bpb = bpb;
	fs->fat32_begin_sector = fat_begin_sector;
	fs->fat32_begin_cluster = fat_begin_cluster;
	fs->fat32_cluster_size = fat_cluster_size;
	vfs_node_t *root = kmalloc(sizeof(vfs_node_t));
	root->fs = fat32;
	root->device = fs;
    vfs_node_t *test = vfs_creat(vfs_getRoot(),"f",VFS_DIRECTORY);
    if (test) {
        test->fs = fat32;
        test->device = fs;
    }
	return root;
}


static struct dirent *fat32_readdir(vfs_node_t *node,uint32_t index) {
	kprintf("FAT32: Under developerment!\n");
	return NULL;
}

static void module_main() {
	// register FS
	fat32 = kmalloc(sizeof(vfs_fs_t));
	fat32->fs_name = "fat32";
	fat32->mount = fat32_mount;
    fat32->readdir = fat32_readdir;
	vfs_addFS(fat32);
	kprintf("FAT32 added\n");
}
