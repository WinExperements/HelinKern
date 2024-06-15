#include <typedefs.h>
#include <vfs.h>
#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <arch/mmu.h>
#include <iso9660/iso9660.h>
#include <arch.h>
//__attribute__((section(".modname"))) char *name = "iso9660";
#define CDROM_SECTOR	2048
// ISO 9660 structures.
// Primary Volume Descriptor.
typedef struct prim_desc {
	int8_t type;
	char ident[5];
	int8_t version;
	char unused1;
	char systemIdent[32];
	char volumeIdent[32];
	char unsued2[8];
	uint64_t vol_space_size;
	char unused3[32];
	int32_t volume_set_size;
	int32_t volume_sequence_number;
	int32_t logical_block_size;
	uint64_t path_table_size;
	int32_t loc_l_path_table;
	int32_t loc_opt_l_path_table;
	int32_t loc_m_type_path_table;
	int32_t loc_opt_m_type_path_table;
	char directory_entry[34];
	char volume_set_ident[128];
	char publisher_ident[128];
	char data_preparer_ident[128];
	char application_ident[128];
	char copyright_file_ident[37];
	char abstract_file_ident[37];
	char bibiographic_file_ident[37];
	char volume_creation_date[17];
	char volume_modification[17];
	char volume_exper_date[17];
	char volume_effective_date[17];
	int8_t file_structure_version;
	char unused4;
	char application_used[512];
	char reserved[653];
} __attribute__((packed)) PrimVolumeDesc;
typedef struct {
	uint8_t length;
	uint8_t ext_length;

	uint32_t extent_start_LSB;
	uint32_t extent_start_MSB;

	uint32_t dataSize_l;
	uint32_t dataSize_h;

	char record_date[7];

	uint8_t flags;
	uint8_t interleave_units;
	uint8_t interleave_gap;

	uint16_t volume_seq_LSB;
	uint16_t volume_seq_MSB;

	uint8_t name_len;
	char name[];
} __attribute__((packed)) DirectoryRecord;

typedef struct pathT {
	char len;
	char extendAttr;
	int32_t lba_extend;
	int16_t count;
	char dirName[20];
} PathTable;
static vfs_fs_t *iso9660_fs = NULL;
// ISO9660 FS specific functions.
static struct dirent *ret_dirent = NULL;
void convertToLower(char *str) {
	for (; *str; ++str) {
		*str = tolower(*str);
	}
}
static struct dirent *iso9660_readdir(vfs_node_t *node,unsigned int index) {
	//arch_sti();
	int i = 0;
	int size = (node->size > 0 ? node->size : CDROM_SECTOR);
	void *sectorBuffer = kmalloc(size);
	int offset = (int)sectorBuffer;
	void *aspace = arch_mmu_getAspace();
	arch_mmu_switch(arch_mmu_getKernelSpace());
	for (int i = 0; i < size / CDROM_SECTOR; i++) {
		vfs_readBlock((vfs_node_t *)node->device,node->inode+i,CDROM_SECTOR,(void *)offset);
		offset+=CDROM_SECTOR;
	}
	arch_mmu_switch(aspace);
	if (ret_dirent == NULL) {
		ret_dirent = sbrk(thread_getThread(thread_getCurrent()),sizeof(struct dirent));
		memset(ret_dirent,0,sizeof(struct dirent));
	}
	offset = (int)sectorBuffer;
	while(1) {
		DirectoryRecord *rec = (DirectoryRecord *)offset;
		if (rec->length == 0) {
			if ((offset - (int)sectorBuffer) < node->size) {
				offset += 1;
				goto retry;
			}
			kfree(sectorBuffer);
			return NULL;
		}
		if (!(rec->flags & 0x01)) {
			if (rec->extent_start_LSB == node->inode) goto search;
			if (i == index) {
				// Okay, we are here.
				memcpy(ret_dirent->name,rec->name,rec->name_len);
				ret_dirent->name[rec->name_len] = 0;
				if (strchr(ret_dirent->name,'.')) {
					ret_dirent->name[rec->name_len-2] = 0;
					if (ret_dirent->name[rec->name_len-3] == '.') {
						ret_dirent->name[rec->name_len-3] = 0;
					}
				}
				convertToLower(ret_dirent->name);
				ret_dirent->node = rec->extent_start_LSB;
				kfree(sectorBuffer);
				return ret_dirent;
			} else {
				i++;
			}
		}
search:
		offset += rec->length;
retry:
		if ((offset - (int)sectorBuffer) >= node->size) {break;}
	}
	kfree(sectorBuffer);
	return NULL;

}
vfs_node_t *iso9660_finddir(vfs_node_t *node,char *f_name) {
	/* Just like iso9660_readdir */
	//arch_sti();
	void *sectorBuffer = kmalloc(node->size);
	void *aspace = arch_mmu_getAspace();
	arch_mmu_switch(arch_mmu_getKernelSpace());
	vfs_readBlock(node->device,node->inode,node->size,sectorBuffer);
	arch_mmu_switch(aspace);
	int offset = (int)sectorBuffer;
	while(1) {
		DirectoryRecord *rec = (DirectoryRecord *)offset;
		if (rec->length == 0) {
			if ((offset - (int)sectorBuffer) < node->size) {
				offset += 1;
				goto retry;
			}
			kfree(sectorBuffer);
			return NULL;
		}
		if (rec->extent_start_LSB == node->inode) {
			goto search;
		}
		if ((rec->flags & 0x01) == 0x01) goto search;
		char name[128];
		// Format the name.
		memcpy(name,rec->name,rec->name_len);
		name[rec->name_len] = 0;
		if (strchr(name,'.')) {
			name[rec->name_len-2] = 0;
			if (name[rec->name_len-3] == '.') {
				name[rec->name_len-3] = 0;
			}
		}
		convertToLower(name);
		if (strcmp(name,f_name)) {
			// Create inode.
			vfs_node_t *ret = kmalloc(sizeof(vfs_node_t));
			memset(ret,0,sizeof(vfs_node_t));
			ret->size = rec->dataSize_l;
			ret->inode = rec->extent_start_LSB;
			ret->name = strdup(name);
			ret->fs = iso9660_fs;
			ret->device = node->device;
			ret->fs_mountOptions = (void *)10;
			if (!(rec->flags & 0x02)) {
				ret->flags = VFS_DIRECTORY;
			}
			return ret;
		}
search:
		offset += rec->length;
retry:
		if ((offset - (int)sectorBuffer) >= node->size) {break;}
	}
	kfree(sectorBuffer);
	return NULL;
}
void iso9660_close(vfs_node_t *node) {
	if ((int)node->fs_mountOptions != 10) return;
	kfree(node->name);
	kfree(node);
}
void cdrom_readBlock(vfs_node_t *from,int blockNo,void *buffer) {
	void *aspace = arch_mmu_getAspace();
	arch_mmu_switch(arch_mmu_getKernelSpace());
	vfs_readBlock(from,blockNo,CDROM_SECTOR,buffer);
	arch_mmu_switch(aspace);
}
int iso9660_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
	//arch_sti();
	int offsetIndex = offset / CDROM_SECTOR;
	int remainOffset = offset % CDROM_SECTOR;
	int remain = size % CDROM_SECTOR;
	int dataBlock = node->inode + offsetIndex;
	//kprintf("Data block: %d, offset: %d\r\n",dataBlock,remainOffset);
	int sectors = size / CDROM_SECTOR;
	void *sector = kmalloc(CDROM_SECTOR);
	if (sectors == 0) {
		void *aspace = arch_mmu_getAspace();
		arch_mmu_switch(arch_mmu_getKernelSpace());
		vfs_readBlock((vfs_node_t*)node->device,dataBlock,2048,sector);
		arch_mmu_switch(aspace);
		memcpy(buffer,sector+remainOffset,size);
		kfree(sector);
		return size;
	}
	vfs_node_t *from = (vfs_node_t *)node->device;
	if (remainOffset != 0) {
		cdrom_readBlock(from,dataBlock,sector);
		int scopy = remainOffset;
		if (scopy == 0) {
			scopy = CDROM_SECTOR;
		}
		if (scopy+remainOffset >= CDROM_SECTOR) {
			scopy = CDROM_SECTOR - remainOffset;
		}
		memcpy(buffer,sector+remainOffset,scopy);
		size-=scopy;
		dataBlock++;
		buffer+=scopy;
		if (scopy+remainOffset < CDROM_SECTOR) {
			int how = CDROM_SECTOR - (scopy+remainOffset);
			if (how < size) {
				memcpy(buffer,sector+(scopy+remainOffset),how);
				buffer+=how;
				size-=how;
				remain+=remainOffset;
				remainOffset = 0;
			}
		}
		sectors--;
		if (remainOffset*2 >= CDROM_SECTOR) {
			remainOffset = 0;
		}
	}
	while(sectors != 0) {
		cdrom_readBlock(from,dataBlock,sector);
		memcpy(buffer,sector,CDROM_SECTOR);
		dataBlock++;
		sectors--;
		buffer += CDROM_SECTOR;
		size-=CDROM_SECTOR;
	}
	if (remain != 0) {
		cdrom_readBlock(from,dataBlock,sector);
		void *sectorRem = sector+remainOffset;
		//kprintf("Remain copying into 0x%x from 0x%x with size %d, offset: %d\r\n",buffer,sectorRem,size,remainOffset);
		memcpy(buffer,sectorRem,size);
		size-=remain;
		buffer+=remain;
		if (size != 0) {
			if (size+remain > CDROM_SECTOR) {
				int how = CDROM_SECTOR - remain;
				memcpy(buffer,sector+remain,how);
				dataBlock++;
				buffer+=how;
				size-=how;
				cdrom_readBlock(from,dataBlock,sector);
				memcpy(buffer,sector,size);
			}
		}
	}
	kfree(sector);
	return size;
}

bool iso9660_mount(vfs_node_t *dev,vfs_node_t *mountPoint,void *parameters) {
	PrimVolumeDesc *desc = kmalloc(sizeof(PrimVolumeDesc));
	memset(desc,0,sizeof(PrimVolumeDesc));
	vfs_readBlock(dev,0x10,1,desc);
	if (desc->type != 0x1) {
		kprintf("iso9660: currently only Primary Volume Descriptor supported\r\n");
		kfree(desc);
		return false;
	}
	if (desc->ident[0] != 'C' ||
			desc->ident[1] != 'D' ||
			desc->ident[2] != '0' ||
			desc->ident[3] != '0' ||
			desc->ident[4] != '1') {
		kprintf("iso9660: wrong descriptor identify\r\n");
		kfree(desc);
		return false;
	}
	DirectoryRecord *root = (DirectoryRecord *)&desc->directory_entry;
	int inode = root->extent_start_LSB;
	kfree(desc);
	mountPoint->fs = iso9660_fs;
	mountPoint->mount_flags = VFS_MOUNT_RO; // ISO9660 always read-only.
	mountPoint->device = dev;
	mountPoint->inode = inode;
	mountPoint->size = root->dataSize_l;
	return true;
}

void iso9660_init() {
	kprintf("ISO9660 device driver\r\n");
	iso9660_fs = kmalloc(sizeof(vfs_fs_t));
	memset(iso9660_fs,0,sizeof(vfs_fs_t));
	iso9660_fs->fs_name = strdup("iso9660");
	iso9660_fs->mount = iso9660_mount;
	iso9660_fs->readdir = iso9660_readdir;
	iso9660_fs->finddir = iso9660_finddir;
	iso9660_fs->close = iso9660_close;
	iso9660_fs->read = iso9660_read;
	vfs_addFS(iso9660_fs);
	kprintf("Registred\r\n");
}
