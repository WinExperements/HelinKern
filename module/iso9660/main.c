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
	char dirName[];
} PathTable;
// Used  by priv_data.
typedef struct _cacheDir {
  vfs_node_t *cacheNode;
} CacheDir;
static vfs_fs_t *iso9660_fs = NULL;
// ISO9660 FS specific functions.
static struct dirent *ret_dirent = NULL;
void convertToLower(char *str) {
	for (; *str; ++str) {
		*str = tolower(*str);
	}
}
static vfs_node_t *cacheNode = NULL;
static int iso9660_readdir(vfs_node_t *node,unsigned int index,struct dirent *to) {
	//arch_sti();
	int i = 0;
	int size = (node->size > 0 ? node->size : CDROM_SECTOR);
	void *sectorBuffer = kmalloc(size);
	vaddr_t offset = (vaddr_t)sectorBuffer;
	for (int i = 0; i < size / CDROM_SECTOR; i++) {
		if (!vfs_readBlock((vfs_node_t *)node->device,node->inode+i,CDROM_SECTOR,(void *)offset)) {
			return NULL;
		}
		offset+=CDROM_SECTOR;
	}
	offset = (vaddr_t)sectorBuffer;
	while(1) {
		DirectoryRecord *rec = (DirectoryRecord *)offset;
		if (rec->length == 0) {
			if ((offset - (vaddr_t)sectorBuffer) < node->size) {
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
				memcpy(to->name,rec->name,rec->name_len);
				to->name[rec->name_len] = 0;
				if (strchr(to->name,'.')) {
					to->name[rec->name_len-2] = 0;
					if (to->name[rec->name_len-3] == '.') {
						to->name[rec->name_len-3] = 0;
					}
				}
				convertToLower(to->name);
				to->node = rec->extent_start_LSB;
				kfree(sectorBuffer);
				return 1;
			} else {
				i++;
			}
		}
search:
		offset += rec->length;
retry:
		if ((offset - (vaddr_t)sectorBuffer) >= node->size) {break;}
	}
	kfree(sectorBuffer);
	return 0;

}
static struct tm tm;
vfs_node_t *iso9660_finddir_real(vfs_node_t *node,char *f_name) {
	/* Just like iso9660_readdir */
	void *sectorBuffer = kmalloc(node->size);
	if (!vfs_readBlock(node->device,node->inode,node->size,sectorBuffer)) {
		return NULL;
	}
	vaddr_t offset = (vaddr_t)sectorBuffer;
	while(1) {
		DirectoryRecord *rec = (DirectoryRecord *)offset;
		if (rec->length == 0) {
			if ((offset - (vaddr_t)sectorBuffer) < node->size) {
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
		int cmplen = rec->name_len;
		// Format the name.
		memcpy(name,rec->name,rec->name_len);
		name[rec->name_len] = 0;
		if (strchr(name,'.')) {
			name[rec->name_len-2] = 0;
			cmplen-=2;
			if (name[rec->name_len-3] == '.') {
				name[rec->name_len-3] = 0;
			}
		}
		convertToLower(name);
		if (!strncmp(name,f_name,cmplen)) {
			// Create inode.
			vfs_node_t *ret = kmalloc(sizeof(vfs_node_t));
			memset(ret,0,sizeof(vfs_node_t));
			ret->size = rec->dataSize_l;
			ret->inode = rec->extent_start_LSB;
			ret->name = strdup(name);
			ret->fs = iso9660_fs;
			ret->device = node->device;
			ret->fs_mountOptions = (void *)10;
			ret->mask = ((S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH) | S_IXOTH);
			// Convert date?
			tm.tm_year = 1900 + rec->record_date[0] - 1;
			tm.tm_mon = rec->record_date[1];
			tm.tm_mday = rec->record_date[2];
			tm.tm_hour = rec->record_date[3];
			tm.tm_min = rec->record_date[4];
			tm.tm_sec = rec->record_date[5];
			ret->ctime = clock_convertTime(&tm);
			ret->mtime = ret->atime = ret->ctime;
			if ((rec->flags & 2) == 2) {
				ret->flags = VFS_DIRECTORY;
                                // Directory? Allocate the cache.
                                CacheDir *cacheDir = kmalloc(sizeof(CacheDir));
                                memset(cacheDir,0,sizeof(CacheDir));
                                ret->priv_data = cacheDir;
			}
			return ret;
		}
search:
		offset += rec->length;
retry:
		if ((offset - (vaddr_t)sectorBuffer) >= node->size) {break;}
	}
	kfree(sectorBuffer);
	return NULL;
}
static vfs_node_t *iso9660_finddir(vfs_node_t *dir,char *f_name) {
  // Check for cache existence.
  CacheDir *cache = (CacheDir *)dir->priv_data;
  if (cache->cacheNode == NULL) {
    // Cache was NEVER initialized. Initialize it and call iso9660_finddir_real.
    cache->cacheNode = kmalloc(sizeof(vfs_node_t));
    memset(cache->cacheNode,0,sizeof(vfs_node_t));
    vfs_node_t *realNode = iso9660_finddir_real(dir,f_name);
  searchInISO:
    if (realNode == NULL) {
      return NULL;
    } else {
      memcpy(cache->cacheNode,realNode,sizeof(vfs_node_t));
      kfree(realNode);
      return cache->cacheNode;
    }
  } else if (cacheNode->name == NULL) {
    // Cache node WAS initialized, but the required node was NOT found in the ISO9660 itself. Repeat process.
    goto searchInISO;
  } else {
    // REAL Fallback?
    // Scan the cache directory.
    vfs_node_t *start = cache->cacheNode;
    while(start != NULL) {
      if (strcmp(start->name,f_name)) {
        return start;
      }
      start = start->next_child;
    }
    // Not found in cache. Try to found inside iso9660.
    vfs_node_t *realNode = iso9660_finddir_real(dir,f_name);
    if (realNode == NULL) {
      return NULL;
    }
    vfs_node_t *newCache = kmalloc(sizeof(vfs_node_t));
    memset(newCache,0,sizeof(vfs_node_t));
    memcpy(newCache,realNode,sizeof(vfs_node_t));
    kfree(realNode);
    // Put into cache.
    start = cache->cacheNode;
    while(start->next_child != NULL) {
      start = start->next_child;
    }
    start->next_child = newCache;
    return newCache;
  }
  // Nather in the cache nather in the ISO itself.
  return NULL;
}
void iso9660_close(vfs_node_t *node) {

}
bool cdrom_readBlock(vfs_node_t *from,int blockNo,void *buffer) {
	bool ret = vfs_readBlock(from,blockNo,CDROM_SECTOR,buffer);
	return ret;
}
uint64_t iso9660_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
  if (offset > node->size) return 0;
	int offsetIndex = offset / CDROM_SECTOR;
	int remainOffset = offset % CDROM_SECTOR;
	int remain = size % CDROM_SECTOR;
	int dataBlock = node->inode + offsetIndex;
	//kprintf("Data block: %d, offset: %d\r\n",dataBlock,remainOffset);
	int sectors = size / CDROM_SECTOR;
	void *sector = kmalloc(CDROM_SECTOR);
  if (size > node->size) {size = node->size;}
  uint64_t rSize = size;
	if (sectors == 0) {
		// Stop! Align only what remain!
		int realSize = min(node->size,size);
		if (!vfs_readBlock((vfs_node_t*)node->device,dataBlock,2048,sector)) {
			kfree(sector);
			return -1;
		}
		memcpy(buffer,sector+remainOffset,realSize);
		kfree(sector);
		return realSize;
	}
	vfs_node_t *from = (vfs_node_t *)node->device;
	if (remainOffset != 0) {
		if (!cdrom_readBlock(from,dataBlock,sector)) {
			kfree(sector);
			return -1;
		}
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
		if (!cdrom_readBlock(from,dataBlock,sector)) {
			kfree(sector);
			return -1;
		}
		memcpy(buffer,sector,CDROM_SECTOR);
		dataBlock++;
		sectors--;
		buffer += CDROM_SECTOR;
		size-=CDROM_SECTOR;
	}
	if (remain != 0) {
		if (!cdrom_readBlock(from,dataBlock,sector)) {
			return -1;
		}
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
				if (!cdrom_readBlock(from,dataBlock,sector)) {
					kfree(sector);
					return -1;
				}
				memcpy(buffer,sector,size);
			}
		}
	}
	kfree(sector);
	return rSize;
}

bool iso9660_mount(vfs_node_t *dev,vfs_node_t *mountPoint,void *parameters) {
	PrimVolumeDesc *desc = kmalloc(sizeof(PrimVolumeDesc));
	memset(desc,0,sizeof(PrimVolumeDesc));
	if (!vfs_readBlock(dev,0x10,CDROM_SECTOR,desc)) {
		kprintf("Target device I/O error\r\n");
		return false;
	}
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
        kprintf("iso9660: volume %s on %s mounted successfully.\n",desc->volumeIdent,dev->name);
	DirectoryRecord *root = (DirectoryRecord *)&desc->directory_entry;
	int inode = root->extent_start_LSB;
	kfree(desc);
        CacheDir *c = kmalloc(sizeof(CacheDir));
        memset(c,0,sizeof(CacheDir));
	mountPoint->fs = iso9660_fs;
	mountPoint->mount_flags = VFS_MOUNT_RO; // ISO9660 always read-only.
	mountPoint->device = dev;
	mountPoint->inode = inode;
	mountPoint->size = root->dataSize_l;
        mountPoint->priv_data = c;
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
