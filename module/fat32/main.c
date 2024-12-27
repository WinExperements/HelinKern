#include <typedefs.h>
#include <mm/alloc.h>
#include <output.h>
#include <vfs.h>
#include <lib/string.h>
#include <lib/queue.h>
#include <arch/mmu.h>
#include <arch.h>
__attribute__((section(".modname"))) char name[] = "fat32";
// FAT32 structures.
// Boot block.
#define FAT_ATTR_RONLY		0x01
#define FAT_ATTR_HIDDEN		0x02
#define FAT_ATTR_SYSTEM		0x04
#define FAT_ATTR_VOL_ID		0x08
#define FAT_ATTR_DIR		0x10
#define FAT_ATTR_ARCHIVE	0x20
#define FAT_ATTR_LFN		0x0F
typedef struct fat_BS
{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short 	        bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		table_count;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16;
	unsigned char		media_type;
	unsigned short		table_size_16;
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32;

	//this will be cast to it's specific type once the driver actually knows what type of FAT this is.
	unsigned char		extended_section[54];

}__attribute__((packed)) BootBlock;

// extended info.
typedef struct _fat32_extend {
	unsigned int		table_size_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
} __attribute__((packed)) FAT32Extended;

typedef struct _dirEntry {
	uint8_t name[11];
	uint8_t attr;
	uint8_t reserved;
	uint8_t creat_time_s;
	uint16_t creat_time;
	uint16_t creat_date;
	uint16_t accessedDate;
	uint16_t first_cluster_high;
	uint16_t modify_time;
	uint16_t modify_date;
	uint16_t first_cluster_low;
	uint32_t size;
} __attribute__((packed)) DirectoryEntry;

typedef struct _long_n {
	uint8_t		order;
	uint16_t	name_1[5];
	uint8_t		attr;
	uint8_t		entry_type;
	uint8_t		checksum;
	uint16_t	name_2[6];
	uint16_t	zero;
	uint16_t	name_3[2];
} __attribute__((packed)) LongNameEntry;

typedef struct _fat_info {
	BootBlock *bpb;
	uint32_t fat_size;
	uint32_t first_data_block;
	vfs_node_t *dev;
	uint32_t cluster_size;
	uint32_t *fat; // file allocation table itself.
	uint32_t fat_sector;
} FatInfo;

typedef struct _fat_file {
	FatInfo 	*fat_ptr;	// pointer to FAT info.
	DirectoryEntry	*dentry;
	int		dir_cluster;	// cluster id where file is stored.
	int		dir_offset;	// offset in cluster where the record is stored.
	bool		is_root;	// is root?
	bool		is_lfn;		// is long name entry?
	queue_t		*inode_cache;
	queue_t		*parent_cache;	// used by fat_rm
} FatFile;

static vfs_fs_t *fatFS;

static bool fat_mount(vfs_node_t *dev,vfs_node_t *mnt,void *flags) {
	BootBlock *bpb = (BootBlock *)kmalloc(sizeof(BootBlock));
	if (!vfs_readBlock(dev,0,sizeof(BootBlock),bpb)) {
		kfree(bpb);
		return false;
	}
	FAT32Extended *ext = (FAT32Extended*)&bpb->extended_section;
	if (ext->boot_signature != 0x28 && ext->boot_signature != 0x29) {
		kprintf("Invalid signature(0x%x expected 0x28 or 0x29)\n",ext->boot_signature);
		kfree(bpb);
		return false;
	}
	// Calculate some critical values for custom data.
	FatInfo *finfo = kmalloc(sizeof(FatInfo));
	memset(finfo,0,sizeof(FatInfo));
	finfo->bpb = bpb;
	finfo->fat_size = ext->table_size_32;
	finfo->first_data_block = bpb->reserved_sector_count + (bpb->table_count * finfo->fat_size);
	finfo->dev = dev;
	finfo->cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
	finfo->fat = kmalloc(finfo->cluster_size);
	finfo->fat_sector = bpb->reserved_sector_count;
	// read cluster with the fat.
	vfs_readBlock(dev,bpb->reserved_sector_count,finfo->cluster_size,finfo->fat);
	FatFile *rootFile = kmalloc(sizeof(FatFile));
	memset(rootFile,0,sizeof(FatFile));
	rootFile->fat_ptr = finfo;
	rootFile->is_root = true;
	mnt->priv_data = rootFile;
	mnt->fs = fatFS;
	return true;
}

static uint32_t cluster_to_lba(uint32_t cluster,FatInfo *fat) {
	return ((cluster-2) * fat->bpb->sectors_per_cluster) + fat->first_data_block;
}
static void fat_convertName(char *fat,char *to) {
	int i = 0;
	for (; *fat; ++fat,++to) {
		if (*fat == ' ') {
			*to = 0;
			break;
		}
		*to = tolower(*fat);
	}
}

static uint32_t fat_nextCluster(FatInfo *finfo,uint32_t currentCluster) {
	// So. Convert currentCluster to id within the FAT table.
	// Convert cluster id into cluster id for fat.
	if (currentCluster < 0) return 0xFFFFFFF8;
	uint32_t fatIndex = currentCluster * 4;
	uint32_t fatSector = fatIndex / finfo->bpb->bytes_per_sector;
	fatSector+=finfo->bpb->reserved_sector_count;
	// fat cache size equals to one cluster.
	if (fatSector > finfo->fat_sector+finfo->bpb->sectors_per_cluster ||
			fatSector < finfo->fat_sector) {
	//	kprintf("%s: reading next part of the FAT\n",__func__);
		// update the cache.
		// align.
		int origIndex = fatIndex;
		fatIndex -= fatIndex % finfo->cluster_size;
		fatSector = fatIndex / finfo->bpb->bytes_per_sector;
		fatSector += finfo->bpb->reserved_sector_count;
		if (!vfs_readBlock(finfo->dev,fatSector,finfo->cluster_size,finfo->fat)) {
			return 0xFFFFFFF8;
		}
		finfo->fat_sector = fatSector;
		fatIndex = origIndex;
	}
	fatSector -= finfo->bpb->reserved_sector_count;
	//fatIndex -= (fatSector * 512);
	// So. Main code.
	int indx = (fatIndex / 4) % 1024;
	uint32_t r = finfo->fat[indx];
	return r;
}
// Find free cluster and link it.
static uint32_t fat_allocateCluster(FatInfo *finfo,uint32_t currentCluster,bool update) {
	// Start from current FAT cache then if not found load new and repeat process. IF no free clusters found, return -1 as
	// failure status.
	// First on, we need to reload cache for our currentCluster.
	if (currentCluster < 0) {
		// What?
		return -1;
	}
	// завантажемо правильну чатину FAT базуючись на currentCluster, якщо потрібно.
	int fatIndex = currentCluster * 4;
	int fatSector = fatIndex / finfo->bpb->bytes_per_sector;
	fatSector+=finfo->bpb->reserved_sector_count;
	int oldFatsector = finfo->fat_sector;
	if (fatSector > finfo->fat_sector+finfo->bpb->sectors_per_cluster ||
			fatSector < finfo->fat_sector) {
		// Update fat cache.
		// align.
		int origIndex = fatIndex;
		fatIndex -= fatIndex % 1024;
		fatSector = fatIndex / finfo->bpb->bytes_per_sector;
		fatSector += finfo->bpb->reserved_sector_count;
		if (!vfs_readBlock(finfo->dev,fatSector,finfo->cluster_size,finfo->fat)) {
			return 0xFFFFFFF8;
		}
		finfo->fat_sector = fatSector; // Це скаже іншій частині драйверу, що ми оновили кеш.
		fatIndex = origIndex;
	}
	fatSector -= finfo->bpb->reserved_sector_count;
	int retClstr = currentCluster+1;
	int clstr = (fatIndex / 4) % 1024;
	clstr+=1;
	int found = 0;
	// Тепер шукаємо
	while(found == 0) {
		for (; clstr < 1024; clstr++,retClstr++) {
			if (finfo->fat[clstr] == 0) {
				// Знайшои!
				finfo->fat[clstr] = 0xFFFFFFF8; // Кінець списку.
				// Запишемо кеш назад.
				if ((fatSector % finfo->bpb->sectors_per_cluster) != 0) {
					fatSector -= fatSector % finfo->bpb->sectors_per_cluster;
				}
				if (!vfs_writeBlock(finfo->dev,fatSector+finfo->bpb->reserved_sector_count,finfo->cluster_size,finfo->fat)) {
					return 0xFFFFFFF8;
				}
				found = retClstr;
				break;
			}
		}
		if (found != 0) break;
		// Завантажимо інший кластер з таблицею FAT.
		fatSector+=finfo->bpb->sectors_per_cluster;
		if (!vfs_readBlock(finfo->dev,fatSector+finfo->bpb->reserved_sector_count,finfo->cluster_size,finfo->fat)) {
			return 0xFFFFFFF8;
		}
		clstr = 0;
	}
	// Завантажимо стару частину FAT для оновлення.
	if (!vfs_readBlock(finfo->dev,oldFatsector,finfo->cluster_size,finfo->fat)) {
		return 0xFFFFFFF8;
	}
	int indx = (fatIndex / 4) % 1024;
	if (update) {
		//kprintf("fat: setting sjfasj for clusat: %d\n",indx);
		finfo->fat[indx] = found;
		if (!vfs_writeBlock(finfo->dev,oldFatsector,finfo->cluster_size,finfo->fat)) {
			return 0xFFFFFFF8;
		}
	}
	return found;

}
void fat_convertFromLong(char to[128],LongNameEntry *from) {
        for (int i = 0; i < 5; i++) {
                if (from->name_1[i] == 65535) {
                        to[i] = 0;
                        return;
                }
                to[i] = (char)from->name_1[i];
        }
        for (int i = 0,n = 5; i < 6; i++,n++) {
                if (from->name_2[i] == 65535) {
                        to[n] = 0;
                        return;
                }
                to[n] = from->name_2[i];
        }
        for (int i = 0,n = 11; i < 2; i++,n++) {
                if (from->name_3[i] == 65535) {
                        to[n] = 0;
                        return;
                }
                to[n] = (char)from->name_3[i];
        }
}
int fat_readdir(vfs_node_t *node,uint32_t dIndex,struct dirent *to) {
	FatFile *f = (FatFile *)node->priv_data;
	uint32_t data_cluster = 0;
	if (f->is_root) {
		// Root directory. We need to retrive the root cluster differently.
		FAT32Extended *ext = (FAT32Extended *)&f->fat_ptr->bpb->extended_section;
		data_cluster = ext->root_cluster;
	} else {
		data_cluster = ((uint32_t)f->dentry->first_cluster_high << 16) | f->dentry->first_cluster_low;
	}
	// Start reading!
	uint32_t lba = cluster_to_lba(data_cluster,f->fat_ptr);
	// Read one cluster. At least try to. TODO: ADD CLUSTER CACHE. OPTMIZE THIS SHIT.
	char *cluster_data = kmalloc(f->fat_ptr->cluster_size);
	vfs_readBlock(f->fat_ptr->dev,lba,f->fat_ptr->cluster_size,cluster_data);
	int i = 0;
	int cluster_ptr = 0;
	while(cluster_data[cluster_ptr] != 0x0) {
                LongNameEntry *lng = NULL;
		if (cluster_data[cluster_ptr] == -27) {
			cluster_ptr+=32;
			continue;
		}
		if (cluster_data[cluster_ptr+11] == 0x0F) {
			// Skip this entry.
			lng = (LongNameEntry *)cluster_data + cluster_ptr;
			cluster_ptr += 32;
			if (cluster_data[cluster_ptr] == 0x0 || cluster_data[cluster_ptr] == -27) {
				break;
			}
		}
		uintptr_t addr = (uintptr_t)cluster_data;
		addr+=cluster_ptr;
		DirectoryEntry *ent = (DirectoryEntry *)addr;
		if (i == dIndex) {
			// Found!
                        if (lng != NULL) {
                                // Convert from long file name entry.
                                fat_convertFromLong(to->name,lng);
                        } else {
			        fat_convertName(ent->name,to->name);
                        }
			kfree(cluster_data);
			return 1;
		}
		cluster_ptr+=32;
		i++;
	}
	kfree(cluster_data);
	return 0;
}
static void fat_readCluster(FatInfo *fs,uint32_t clusterID,void *buffer);
static vfs_node_t *fat_finddir(vfs_node_t *node,char *name) {
	// first. check the cache.
	FatFile *file = (FatFile *)node->priv_data;
	if (file->inode_cache == NULL) {
		file->inode_cache = queue_new();
	}
	queue_for(el,file->inode_cache) {
		vfs_node_t *node = (vfs_node_t *)el->value;
		if (strcmp(node->name,name)) {
			return node;
		}
	}
	// Nope? okay...
	// So. Find the data cluster.
	uint32_t data_cluster = 0;
	if (file->is_root) {
		FAT32Extended *ext = (FAT32Extended *)&file->fat_ptr->bpb->extended_section;
		data_cluster = ext->root_cluster;
	} else {
		// Convert.
		data_cluster = ((uint32_t)file->dentry->first_cluster_high << 16) | file->dentry->first_cluster_low;
	}
	uint32_t data_sector = cluster_to_lba(data_cluster,file->fat_ptr);
	char *cluster_data = kmalloc(file->fat_ptr->cluster_size);
	uint32_t cluster_ptr = 0;
	vfs_readBlock(file->fat_ptr->dev,data_sector,file->fat_ptr->cluster_size,cluster_data);
	char cmpname[11];
	bool lfn = false;
	int indx = 1;
	while(cluster_data[cluster_ptr] != 0) {
		if (cluster_data[cluster_ptr] == -27) {
			cluster_ptr+=32;
			continue;
		}
		if (cluster_data[cluster_ptr+11] == 0x0F) {
			// Long name entry.
			// So because the size of long entry is equal to size of standard entry we can skip it.
			cluster_ptr+=32;
			lfn = true;
			continue;
		}
		uintptr_t entPtr = (uintptr_t)cluster_data;
		entPtr+=cluster_ptr;
		DirectoryEntry *ent = (DirectoryEntry *)entPtr;
		fat_convertName(ent->name,cmpname);
		if (strcmp(cmpname,name)) {
			// Create node?
			vfs_node_t *fat_node = kmalloc(sizeof(vfs_node_t));
			memset(fat_node,0,sizeof(vfs_node_t));
			fat_node->name = strdup(cmpname);
			fat_node->size = ent->size;
			fat_node->fs = fatFS;
			fat_node->mask = ((S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH) | S_IXOTH);
			fat_node->inode = data_sector+indx;
                        if ((ent->attr & 0x10) == 0x10) {
                                fat_node->flags = VFS_DIRECTORY;
                        }
			// Parse dates.
			FatFile *finfo = kmalloc(sizeof(FatFile));
			memset(finfo,0,sizeof(FatFile));
			finfo->fat_ptr = file->fat_ptr;
			finfo->dentry = kmalloc(sizeof(DirectoryEntry));
			finfo->dir_cluster = data_cluster;
			finfo->dir_offset = cluster_ptr;
			finfo->parent_cache = file->inode_cache;
			finfo->is_lfn = lfn;
			memcpy(finfo->dentry,ent,sizeof(DirectoryEntry));
			fat_node->priv_data = finfo;
			enqueue(file->inode_cache,fat_node);
			kfree(cluster_data);
			return fat_node;
		}
		cluster_ptr+=32;
		if (cluster_ptr >=file->fat_ptr->cluster_size) {
			// Check for new cluster.
			data_sector = fat_nextCluster(file->fat_ptr,data_sector);
			if (data_sector == 0xFFFFFFF8) {
				break;
			}
			// Found? Read and continue.
			vfs_readBlock(file->fat_ptr->dev,data_sector,file->fat_ptr->cluster_size,cluster_data);
			cluster_ptr = 0;
		}
		indx++;
	}
	kfree(cluster_data);
	return NULL;
}

static uint64_t fat_read(vfs_node_t *node,uint64_t offset,uint64_t size,void *buffer) {
	//kprintf("fat32: fat_read called by %s\n",runningTask->name);
	FatFile *file = (FatFile *)node->priv_data;
	int theFuckingCluster = ((uint32_t)file->dentry->first_cluster_high << 16) | file->dentry->first_cluster_low;
	int startClusterID = theFuckingCluster + (offset / file->fat_ptr->cluster_size);
	int fullSize = offset+size;
	if (offset+size > node->size) {
		fullSize = node->size;
	}
	int dataCluster = startClusterID;
	if (startClusterID > theFuckingCluster) {
		if (fat_nextCluster(file->fat_ptr,startClusterID) == 0xFFFFFFF8) {
			kprintf("fat32: we can think this is end of file already? Next cluster is probably is 0x%x\n",fat_nextCluster(file->fat_ptr,startClusterID+1));
			return -1;
		}
	}
	int endClusterID = theFuckingCluster + (fullSize / file->fat_ptr->cluster_size);
	int oper_size = fullSize - offset;
	int blocks = (fullSize / file->fat_ptr->cluster_size);
	int end_size = fullSize - (blocks * file->fat_ptr->cluster_size);
	int clustr = file->fat_ptr->cluster_size;
	if (size < 0) {
		return -1;
	}
	char *clusterBuffer = kmalloc(file->fat_ptr->cluster_size);
	if (startClusterID == endClusterID) {
		if (!vfs_readBlock(file->fat_ptr->dev,cluster_to_lba(startClusterID,file->fat_ptr),file->fat_ptr->cluster_size,clusterBuffer)) {
			kprintf("fat32: read error!\n");
			return -1;
		}
		if (oper_size < 0) {
			kfree(clusterBuffer);
			return -1;
		}
		memcpy(buffer,(uint8_t *)(((uintptr_t)clusterBuffer) + ((uintptr_t)offset % file->fat_ptr->cluster_size)),oper_size);
		kfree(clusterBuffer);
		return oper_size;
	} else {
		int written = 0;
		for (int i = startClusterID; i < endClusterID; i++,written++) {
			if (i == startClusterID) {
				vfs_readBlock(file->fat_ptr->dev,cluster_to_lba(dataCluster,file->fat_ptr),file->fat_ptr->cluster_size,clusterBuffer);
				memcpy(buffer,(uint8_t*)(((uintptr_t)clusterBuffer) + ((uintptr_t)offset % clustr)),clustr - (offset % clustr));
				dataCluster = fat_nextCluster(file->fat_ptr,dataCluster);
			} else {
				vfs_readBlock(file->fat_ptr->dev,cluster_to_lba(dataCluster,file->fat_ptr),file->fat_ptr->cluster_size,clusterBuffer);
				memcpy(buffer + clustr * written - (offset % clustr),clusterBuffer,clustr);
				dataCluster = fat_nextCluster(file->fat_ptr,dataCluster);
				if (dataCluster == 0xFFFFFFF8) {
					kfree(clusterBuffer);
					return written*clustr;
				}
			}
		}
		if (end_size > 0) {
			fat_readCluster(file->fat_ptr,dataCluster,clusterBuffer);
			memcpy(buffer + file->fat_ptr->cluster_size * written - (offset % clustr),clusterBuffer,end_size);
		}

	}
	kfree(clusterBuffer);
	return oper_size;
}

// write support.
static void fat_convName(char *from,char *to) {
	for (int i = 0; i < 8; i++) {
		if (from[i] == 0) break;
		to[i] = toupper(from[i]);
	}
}

// source: https://gist.github.com/itisravi/4440535

unsigned char
ChkSum (unsigned char *pFcbName)
{
  short FcbNameLen;
  unsigned char Sum;

  Sum = 0;
  for (FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--)
    {
      // NOTE: The operation is an unsigned char rotate right
      Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
  return (Sum);
}


static vfs_node_t *fat_creat(vfs_node_t *in,char *name,int flags) {
	FatFile *file = (FatFile *)in->priv_data;
	// Get the data cluster ID.
	uint32_t clusterID = 0;
	if (file->is_root) {
		FAT32Extended *ext = (FAT32Extended*)&file->fat_ptr->bpb->extended_section;
		clusterID = ext->root_cluster;
	} else {
		clusterID = ((uint32_t)file->dentry->first_cluster_high << 16) | file->dentry->first_cluster_low;
	}
	uint32_t lba = cluster_to_lba(clusterID,file->fat_ptr);
	// Now go into directory like fat_finddir or fat_readdir.
	uint32_t cluster_ptr = 0;
	char *clusterData = kmalloc(file->fat_ptr->cluster_size);
	vfs_readBlock(file->fat_ptr->dev,lba,file->fat_ptr->cluster_size,clusterData);
	while(clusterData[cluster_ptr] != 0 && clusterData[cluster_ptr] != -27) {
		cluster_ptr+=32;
		if (cluster_ptr>=file->fat_ptr->cluster_size) {
			kfree(clusterData);
			kprintf("vfat: check fat_creat.\n");
			return NULL;
		}
	}
	// Now we found where we can put the node.
	// Long name entry.
	int namelen = strlen(name);
	/* the usage of addr is very important here. Don't remove */
	uintptr_t addr = (uintptr_t)clusterData;
	addr+=cluster_ptr;
	LongNameEntry *ent = (LongNameEntry*)addr;
	memset(ent,0,sizeof(LongNameEntry));
	memset(ent->name_1,0xFF,10);
	memset(ent->name_2,0xFF,12);
	memset(ent->name_3,0xFF,4);
	ent->order = 0x41;
	for (int i = 0; i < 5; i++) {
		if (i > namelen) break;
		ent->name_1[i] = name[i];
	}
	if (namelen < 5) {
		ent->name_1[namelen] = 0;
	}
	if (namelen > 5) {
		for (int i = 5; i < 11; i++) {
			if (i > namelen) break;
			ent->name_2[i-5] = name[i];
		}
	}
	ent->attr = 0x0F;
	// Generate checksum.
	addr+=32;
	DirectoryEntry *dentry = (DirectoryEntry *)addr;
	memset(dentry,0,sizeof(DirectoryEntry));
	memset(dentry->name,' ',11);
	dentry->attr = FAT_ATTR_ARCHIVE;
        if (flags == 511) {
                dentry->attr |= 0x10;
                // Allocate the data cluster.
                uint32_t clstr = fat_allocateCluster(file->fat_ptr,4,false);
                dentry->first_cluster_low = (uint16_t)(clstr & 0xFFFF);
                dentry->first_cluster_high = (uint16_t)((clstr >> 16) & 0xFFFF);
                // Write empty data to the cluster.
                void *emptyDir = kmalloc(file->fat_ptr->cluster_size);
                memset(emptyDir,0,file->fat_ptr->cluster_size);
                vfs_writeBlock(file->fat_ptr->dev,cluster_to_lba(clstr,file->fat_ptr),file->fat_ptr->cluster_size,emptyDir);
                kfree(emptyDir);
        }
	fat_convName(name,dentry->name);
	uint8_t checksum = ChkSum(dentry->name);
	unsigned char *nam = dentry->name;
	ent->checksum = checksum;
	vfs_writeBlock(file->fat_ptr->dev,lba,file->fat_ptr->cluster_size,clusterData);
	kfree(clusterData);
	return fat_finddir(in,name);
}

static uint32_t fat_getOrCreateCluster(FatInfo *finfo,uint32_t currentCluster) {
	uint32_t next = fat_nextCluster(finfo,currentCluster);
	//kprintf("cluster %d, next -> %d\n",currentCluster,next);
	if (next >= 0xffffff ||
			next == 0) {
		next = fat_allocateCluster(finfo,currentCluster,true);
		// Double check.
		if (next == 0xFFFFFFF8) {
			return -1;
		}
	}
	return next;
}

// helper functions.
static void fat_readCluster(FatInfo *fs,uint32_t clusterID,void *buffer) {
	uint32_t lba = cluster_to_lba(clusterID,fs);
	vfs_readBlock(fs->dev,lba,fs->cluster_size,buffer);
}
static bool fat_writeCluster(FatInfo *fs,uint32_t clusterID,void *buffer) {
	uint32_t lba = cluster_to_lba(clusterID,fs);
	return vfs_writeBlock(fs->dev,lba,fs->cluster_size,buffer);
}

static uint64_t fat_write(vfs_node_t *node,uint64_t offset,uint64_t size,void *fbuffer) {
	int size_s = (int)size;
	int off = (int)offset;
	FatFile *file = (FatFile *)node->priv_data;
	if (file->dentry->first_cluster_low == 0) {
		// Allocate cluster!
		file->dentry->first_cluster_low = fat_allocateCluster(file->fat_ptr,4,false); // cluster ID 3 is first available. 0 1 and 2 is reserved.
	}
	uint32_t clstr = ((uint32_t)file->dentry->first_cluster_high << 16) | file->dentry->first_cluster_low;
	int startClusterID = clstr + (off / file->fat_ptr->cluster_size);
	//kprintf("file cluster => %d. start cluster id: %d\n",clstr,startClusterID);
	//kprintf("fat_write: offset %d bytes, size: %d. Start cluster: %d\n",offset,size,startClusterID);
	int fullSize = off+size;
	char *clusterdata = kmalloc(file->fat_ptr->cluster_size);
	if (fullSize > node->size) {
		node->size = fullSize;
		file->dentry->size = fullSize;
		if (!vfs_readBlock(file->fat_ptr->dev,cluster_to_lba(file->dir_cluster,file->fat_ptr),file->fat_ptr->cluster_size,clusterdata)) {
			kprintf("fat: warrning: failed to read the entry cluster. The entry metadata can't be updated as same as the data. Abort\n");
			kfree(clusterdata);
			return -1;
		}
		uintptr_t addr = (uintptr_t)clusterdata;
		addr+=file->dir_offset;
		memcpy((void *)addr,file->dentry,32);
		if (!vfs_writeBlock(file->fat_ptr->dev,cluster_to_lba(file->dir_cluster,file->fat_ptr),file->fat_ptr->cluster_size,clusterdata)) {
			kfree(clusterdata);
			kprintf("fat: warrning: failed to write to the entry cluster. Entry metadata can't be updated, write process aborted.\n");
			return -1;
		}
	}
	if (startClusterID > clstr) {
		if (fat_getOrCreateCluster(file->fat_ptr,startClusterID) == 0) {
			kprintf("fat32: unallocated cluster?(%d)\n",startClusterID);
			return -1;
		}
	}
	uint32_t blocks = (fullSize / file->fat_ptr->cluster_size);
	uint32_t endCluster = clstr + (fullSize / file->fat_ptr->cluster_size);
	uint32_t end_size = fullSize - (blocks * file->fat_ptr->cluster_size);
	uint32_t size_write = fullSize - off;
	if (startClusterID == endCluster) {
		// check and allocate RIGHT cluster.
		fat_readCluster(file->fat_ptr,startClusterID,clusterdata);
		if (size_write < 0) {
			kprintf("ERROR!\n");
			kfree(clusterdata);
			return -1;
		}
		//kprintf("fat: size_write = %d\n",size_write);
		memcpy(((uint8_t*)((uintptr_t)clusterdata) + ((uintptr_t)off % file->fat_ptr->cluster_size)),fbuffer,size_write);
		fat_writeCluster(file->fat_ptr,startClusterID,clusterdata);
	} else {
		int blocks_write = 0;
		int fileCluster = startClusterID;
		//kprintf("ff: %d %d %d\n",fileCluster,startClusterID,clstr);
		for (int blkoff = startClusterID; blkoff < endCluster; blkoff++,blocks_write++) {
			if (blkoff == startClusterID) {
				fat_readCluster(file->fat_ptr,fileCluster,clusterdata);
				memcpy((uint8_t*)(((uintptr_t)clusterdata) + ((uintptr_t)off % file->fat_ptr->cluster_size)),fbuffer,file->fat_ptr->cluster_size - (off % file->fat_ptr->cluster_size));
				fat_writeCluster(file->fat_ptr,fileCluster,clusterdata);
				fileCluster = fat_getOrCreateCluster(file->fat_ptr,fileCluster);
			} else {
				fat_readCluster(file->fat_ptr,fileCluster,clusterdata);
				memcpy(clusterdata,fbuffer + (file->fat_ptr->cluster_size * blocks_write) - (off % file->fat_ptr->cluster_size),file->fat_ptr->cluster_size);
				fat_writeCluster(file->fat_ptr,fileCluster,clusterdata);
				fileCluster = fat_getOrCreateCluster(file->fat_ptr,fileCluster);
			}
		}
		if (end_size > 0) {
			fat_readCluster(file->fat_ptr,fileCluster,clusterdata);
			memcpy(clusterdata,fbuffer + file->fat_ptr->cluster_size * blocks_write - (off % file->fat_ptr->cluster_size),end_size);
			//kprintf("writing %d as offset\n",end_size);
			fat_writeCluster(file->fat_ptr,fileCluster,clusterdata);
		}
	}
	kfree(clusterdata);
	return size_write;
}

static bool fat_rm(vfs_node_t *node) {
	FatFile *f = (FatFile *)node->priv_data;
	if (f->is_root) {
		return false;
	}
	// Clean the directory entry.
	uint32_t clusterOffset = f->dir_offset;
	int clearCount = 1;
	if (f->is_lfn) {
		// if the entry itself is lfn. then we mark the lfn entry and the directroy entry itself as DELETED.
		clusterOffset-=32;
		clearCount = 2;
	}
	// Load FAT cache if required.
	if (f->dentry->first_cluster_low != 0) {
		// calculate cluster.
		uint32_t curCluster = ((uint32_t)f->dentry->first_cluster_high << 16) | f->dentry->first_cluster_low;
		uint32_t nextCluster = fat_nextCluster(f->fat_ptr,curCluster);
		while(1) {
			if (curCluster == 0xFFFFFFF8) {
				f->fat_ptr->fat[curCluster] = 0;
				break;
			}
			f->fat_ptr->fat[curCluster] = 0x0;
			curCluster = nextCluster;
			nextCluster = fat_nextCluster(f->fat_ptr,curCluster);
		}
	}
	if (f->parent_cache != NULL) {
		queue_remove(f->parent_cache,node);
	}
	// Free the inode and it properties.
	if (f->inode_cache != NULL) {
		while(dequeue(f->inode_cache) != 0);
		kfree(f->inode_cache);
	}
	kfree(f->dentry);
	kfree(f);
	return true;
}
#ifdef AS_MODULE
static void module_main() {
#else
void fat_init() {
#endif
	fatFS = kmalloc(sizeof(vfs_fs_t));
	memset(fatFS,0,sizeof(vfs_fs_t));
	fatFS->fs_name = "vfat";
	fatFS->mount = fat_mount;
	fatFS->readdir = fat_readdir;
	fatFS->finddir = fat_finddir;
	fatFS->read = fat_read;
	fatFS->creat = fat_creat;
	fatFS->write = fat_write;
	vfs_addFS(fatFS);
}
