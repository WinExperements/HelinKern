#include <output.h>
#include "slimfat/slimfat.h"
#include <vfs.h>
#include <mm/alloc.h>
#include <lib/string.h>

// SlimFAT wrapper
static uint8_t wrapper_read(void *node,const uint32_t sector,uint8_t *buffer) {
	vfs_node_t *n = (vfs_node_t *)node;
	vfs_readBlock(n,sector,512,buffer);
	return 0;
}

static uint8_t wrapper_write(void *node,const uint32_t sector,const uint8_t *buffer) {
	//kprintf("WARRNING: SlimFAT wrapper: wrapper write is experemental!\n");
	vfs_node_t *n = (vfs_node_t *)node;
        vfs_writeBlock(n,sector,512,buffer);
	return 0;
}

static char *slimerr_to_string(int err) {
	switch(err) {
		case FS_SUCCESS:
			return "FS_SUCCESS";
			break;
		case FS_READ_FAIL:
			return "FS_READ_FAIL";
			break;
		case FS_WRITE_FAIL:
			return "FS_WRITE_FAIL";
		case FS_SIG_MISMATCH:
			return "FS_SIG_MISMATCH";
		case FS_UNSUPPORTED_FS:
			return "FS_UNSUPPORTED_FS";
		case FS_END_OF_CHAIN:
			return "FS_END_OF_CHAIN";
		case FS_FILE_NOT_FOUND:
			return "FS_FILE_NOT_FOUND";
		case FS_INVALID_OFFSET:
			return "FS_INVALID_OFFSET";
		case FS_FILE_ACCES_FAIL:
			return "FS_FILE_ACCES_FAIL";
		case FS_UNSUPPORTED_MODE:
			return "FS_UNSUPPORTED_MODE";
		default:
			return "FS_UNKNOWN";
	}
	return "ERR";
}

__attribute__((section(".modname"))) char *name = "mbr";

static vfs_fs_t *fat_fs;
typedef struct fatfs_specifc_data {
	fs_storage_device storage;
	fs_partition_t partition;
	uint8_t buffer[SECTOR_SIZE];
} fatfs_specifc_data_t;

bool slimfat_mount(struct vfs_node *dev,struct vfs_node *mountpoint,void *) {
	fatfs_specifc_data_t *spc = (fatfs_specifc_data_t *)kmalloc(sizeof(fatfs_specifc_data_t));
	memset(spc,0,sizeof(fatfs_specifc_data_t));
	fs_storage_device t =  GET_DEV_HANDLE(spc->buffer, dev, wrapper_read, wrapper_write);
	fs_partition_t part = GET_PART_HANDLE(spc->storage);
	spc->storage = t;
	spc->partition = part;
	int ret = 0;
	if ((ret = fs_mount(&spc->partition,0)) != 0) {
		kprintf("fs_mount returned error: %s\n",slimerr_to_string(ret));
		return false;
	} else {
		kprintf("SlimFAT mounted successfully!\n");
	}
        mountpoint->fs = fat_fs;
	mountpoint->priv_data = spc;
	return true;

}

 int slimfat_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
	uint32_t off = (uint32_t)offset;
	uint32_t s = (uint32_t)how;
	kprintf("%s, off: %d, size: %d\n",__func__,off,s);
	fs_file_t *d = (fs_file_t *)node->priv_data;
	char *b = (char *)buf;
	if (fs_fread(d,b,s)) {
		kprintf("Error reading file from FAT :(\n");
		return 0;
	}
	kprintf("Readed data: %s\n",b);
	return how;
 }

vfs_node_t *slimfat_find(struct vfs_node *in,char *name) {
	kprintf("%s: find %s\n",__func__,name);
	fatfs_specifc_data_t *d = (fatfs_specifc_data_t *)in->priv_data; // root directory
	fs_file_t file =  GET_FILE_HANDLE(d->partition);
	if (fs_fopen(&file,name,READ)) {
		kprintf("%s: failed to open\n",__func__);
		return NULL;
	}
	vfs_node_t *n = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
	memset(n,0,sizeof(vfs_node_t));
	strcpy(n->name,name);
	fs_fseek(&file,0,FS_SEEK_END);
	int size = fs_ftell(&file);
        kprintf("%s: size: %d\n",__func__,size);
        n->size = size;
	fs_fseek(&file,0,FS_SEEK_SET);
	n->priv_data = &file;
	n->fs = fat_fs;
	kprintf("%s: Returning node, size: %d\n",__func__,n->size);
	return n;
}

static void module_main() {
    kprintf("SlimFAT(https://github.com/majcoch/slim-fat-library) port to HelinKern\r\n");
    // We need to register our file system here
	fat_fs = kmalloc(sizeof(vfs_fs_t));
	memset(fat_fs,0,sizeof(vfs_fs_t));
	fat_fs->fs_name = "fat";
	fat_fs->mount = slimfat_mount;
	fat_fs->read = slimfat_read;
	fat_fs->finddir = slimfat_find;
	vfs_addFS(fat_fs);
	kprintf("Filesystem added\n");
}
