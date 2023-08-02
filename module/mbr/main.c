#include<output.h>
#include <mm/alloc.h>
#include <typedefs.h>
#include <vfs.h>
#include <dev.h>
#include "mbr.h"
#include <lib/string.h>
#include <debug.h>
static int global_part_id = 0;
// define our module name in .modname section that needed for our module loader!
__attribute__((section(".modname"))) char *name = "mbr";

static mbr_t m_mbr;

static void mbr_dev_readBlock(vfs_node_t *node,int blockNo,int how,void *buf) {
    DEBUG("MBR read %d sectors\r\n",how/512);
    if (node->device == NULL) {
        kprintf("MBR: %s: no device pointer in inode!\n",node->name);
        return;
    }
    mbr_dev_t *dev = (mbr_dev_t *)node->device;
    if (dev->lba_start == 0) return;
    int off = dev->lba_start;
    off+=blockNo;
    vfs_readBlock((vfs_node_t *)dev->harddrive_addr,off,how,buf);
}

static void mbr_dev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf) {
    if (node->device == NULL) {
        kprintf("MBR: %s: no device pointer in inode!\n",node->name);
        return;
    }
    mbr_dev_t *dev = (mbr_dev_t *)node->device;
    int off = dev->lba_start;
    off+=blockNo;
    DEBUG("MBR: writing %d blocks\r\n",how/512);
    vfs_writeBlock((vfs_node_t *)dev->harddrive_addr,off,how,buf);
}

static int mbr_dev_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buff) {
	// MBR
	if (node->device == NULL) return 0;
	mbr_dev_t *dev = (mbr_dev_t *)node->device;
	int off = dev->lba_start*512;
	return vfs_read((vfs_node_t *)dev->harddrive_addr,off+offset,how,buff);
}

static void mbr_registerDevice(vfs_node_t *harddrive,int part_index,mbr_t *mbr) {
    mbr_dev_t *dev = kmalloc(sizeof(mbr_dev_t));
    memset(dev,0,sizeof(mbr_dev_t));
    dev->harddrive_addr = (int)harddrive;
    dev->part_index = part_index;
    dev->lba_start = mbr->partitions[part_index].lba_first_sector;
    dev->sectors = mbr->partitions[part_index].sector_count;
    dev_t *d = kmalloc(sizeof(dev_t));
    memset(d,0,sizeof(dev_t));
    d->name = kmalloc(6);
    strcpy(d->name,harddrive->name);
    d->name[3] = 'p';
    d->name[4]  =  global_part_id+'0';
    d->name[5] = '\0';
    d->writeBlock = mbr_dev_writeBlock;
    d->readBlock = mbr_dev_readBlock;
    d->read = mbr_dev_read;
    d->device = dev;
    d->buffer_sizeMax = dev->sectors*512;
    dev_add(d);
    global_part_id++;
    kprintf("%s ",d->name);
}

static void mbr_parseMbr(vfs_node_t *harddrive,uint32_t extPartSector,mbr_t mbr) {
    if (mbr.signature[0] == 0x55 && mbr.signature[1] == 0xAA) {
        for (int i = 0; i < 4; i++) {
            if (mbr.partitions[i].sector_count != 0) { //active partiton
                    int off = extPartSector;
                    mbr.partitions[i].lba_first_sector += off;
                    //kprintf("MBR: Partiton %d start: %d, size(in sectors): %d, offset: %d\n",i,mbr.partitions[i].lba_first_sector,mbr.partitions[i].sector_count,off);
                    mbr_registerDevice(harddrive,i,&mbr);
                    if (mbr.partitions[i].type == 0x5 || mbr.partitions[i].type == 0xf) {
                        mbr_t *sec_mbr = kmalloc(sizeof(mbr_t));
                        vfs_readBlock(harddrive,(mbr.partitions[i].lba_first_sector),512,sec_mbr);
                        mbr_parseMbr(harddrive,mbr.partitions[i].lba_first_sector,*sec_mbr);
                        kfree(sec_mbr);
                    }
            }
        }
    } else {
        kprintf("MBR: No valid partition table found\n");
        kprintf("Returned signature: %x, %x\n",mbr.signature[0],mbr.signature[1]);
    }
}

static void module_main() {
    // Currently support only on master drive
    kprintf("MBR module starting up\r\n");
    vfs_node_t *harddrive = vfs_find("/dev/hda");
    if (!harddrive) {
        kprintf("MBR: cannot open hard drive A!\n");
        return; // exit
    }
    // read mbr_t structure
    vfs_readBlock(harddrive,0,512,&m_mbr);
    kprintf("Detected hard drive partitions: ");
    mbr_parseMbr(harddrive,0,m_mbr);
    kprintf("\r\n");
}
mbr_t *mbr_getMBR() {
    return &m_mbr;
}
