#include<output.h>
#include <mm/alloc.h>
#include <typedefs.h>
#include <vfs.h>
#include <dev.h>
#include "mbr/mbr.h"
#include <lib/string.h>
#include <debug.h>
#include <fs/parttab.h>
static int global_part_id = 0;
static char gl_ch = 'a';
/*
 * Master Boot recocord and GUID Partition table driver
 * for HelinOS. Example of Partition Manager interface driver.
 * Written by WinExperements.
*/
// define our module name in .modname section that needed for our module loader!
//__attribute__((section(".modname"))) char *name = "mbr";

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

static void mbr_registerDevice(vfs_node_t *harddrive,int lba_start,uint64_t sectors) {
    mbr_dev_t *dev = kmalloc(sizeof(mbr_dev_t));
    memset(dev,0,sizeof(mbr_dev_t));
    dev->harddrive_addr = (int)harddrive;
    dev->part_index = 0;
    dev->lba_start = lba_start;
    dev->sectors = sectors;
    dev_t *d = kmalloc(sizeof(dev_t));
    memset(d,0,sizeof(dev_t));
    d->name = kmalloc(6);
    strcpy(d->name,harddrive->name);
    d->name[3] = 'p';
    if (global_part_id >= 9) {
        d->name[4] = gl_ch;
        gl_ch++;
    } else {
        d->name[4]  =  global_part_id+'0';
    }
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

static void parseGPT(vfs_node_t *hard) {
	// Yes, we now support GPT partition table
    GPTHeader *header = kmalloc(sizeof(GPTHeader));
    /*
     * Some hard disk drivers can fail for reading the real size of GPT partiton table
     * due to the bug of some functions. So we create an alternative method to do this.
    */
    vfs_read(hard,512,sizeof(GPTHeader),header);
    if (header->signature != GPT_SIGNATURE) {
        kprintf("Invalid GPT header signature\n");
	kfree(header);
	return;
    }
    int start = header->partitionEntryLBA * 512;
    int size = sizeof(GPTPartitionEntry) * header->numberOfPartitionEntries;
    GPTPartitionEntry *array = kmalloc(size);
    vfs_read(hard,start,size,array);
    for (int i = 0; i < header->numberOfPartitionEntries; i++) {
        if (array[i].partitionTypeGUID[0] == 0) {
            continue;
        }
        /*kprintf("Partiton: ");
        uint16_t *buff = array[i].partitionName;
        for (int i = 0; i < 16; i++) {
            kprintf("%c",buff[i]);
        }
        kprintf(" LBA: 0x%x\n",array[i].startingLBA);*/
	uint64_t sizeOfPart = array[i].endingLBA - array[i].startingLBA;
        mbr_registerDevice(hard,array[i].startingLBA,sizeOfPart);
    }
    // Free header taken memory
    kfree(header);
}

static void mbr_parseMbr(vfs_node_t *harddrive,uint32_t extPartSector,mbr_t mbr) {
    if (mbr.signature[0] == 0x55 && mbr.signature[1] == 0xAA) {
        kprintf("Detected partitons: ");
        for (int i = 0; i < 4; i++) {
            if (mbr.partitions[i].sector_count != 0) { //active partiton
		    if (mbr.partitions[i].type == 0xEE) {
			parseGPT(harddrive);
			continue;
		    }
                    int off = extPartSector;
                    mbr.partitions[i].lba_first_sector += off;
                    //kprintf("MBR: Partiton %d start: %d, size(in sectors): %d, offset: %d\n",i,mbr.partitions[i].lba_first_sector,mbr.partitions[i].sector_count,off);
                    mbr_registerDevice(harddrive,mbr.partitions[i].lba_first_sector,(uint64_t)mbr.partitions[i].sector_count);
                    if (mbr.partitions[i].type == 0x5 || mbr.partitions[i].type == 0xf) {
                        mbr_t *sec_mbr = kmalloc(sizeof(mbr_t));
                        vfs_readBlock(harddrive,(mbr.partitions[i].lba_first_sector),512,sec_mbr);
                        mbr_parseMbr(harddrive,mbr.partitions[i].lba_first_sector,*sec_mbr);
                        kfree(sec_mbr);
                    }
            } else {
		    if (mbr.partitions[i].type == 0xEE) {
			    kprintf("mbr: GPT partition table detected\n");
		    }
	    }
        }
        kprintf("\n");
    } else {
        kprintf("MBR: No valid partition table found\n");
        kprintf("Returned signature: %x, %x\n",mbr.signature[0],mbr.signature[1]);
    }
}

bool mbr_parttab_scan(vfs_node_t *hard) {
	// Read the mbr from harddrive
	kprintf("Just experement....");
	mbr_t *test = kmalloc(512);
	vfs_read(hard,0,512,test);
	if (test->signature[0] != 0x55 && test->signature[1] != 0xAA) {
		kprintf("Even after that, signature invalid. Got: 0x%x and 0x%x\r\n",test->signature[0],
											test->signature[1]);
	} else {
		kprintf("Mbr pointer points to valid MBR structure\r\n");
	}
	kfree(test);
	memset(&m_mbr,0,512);
	m_mbr.signature[0] = 0xAB;
	vfs_read(hard,0,512,&m_mbr);
	mbr_parseMbr(hard,0,m_mbr);
	return true;
}

void mbr_init() {
    // Currently support only on master drive
    kprintf("MBR module starting up\r\n");
    // register itself in partTab interface
    partTable_t *mbr = kmalloc(sizeof(partTable_t));
    memset(mbr,0,sizeof(partTable_t));
    mbr->name = "Master Boot Record";
    mbr->search = mbr_parttab_scan;
    partTab_register(mbr);
    kprintf("Registred in partTab interface\n");
}
mbr_t *mbr_getMBR() {
    return &m_mbr;
}
