#include <output.h>
#include <arch/x86/io.h>
#include <atapi/atapi.h>
#include <dev.h>
#include <mm/alloc.h>
#include <lib/string.h>
/*
    Ported version of https://github.com/klange/toaruos/blob/toaru-1.x/modules/ata.c
*/
// === Add module name here ===
char modname[] __attribute__((section(".modname"))) = "atapi";
// === Internal functions here ===
typedef struct {
	uint16_t flags;
	uint16_t unused1[9];
	char     serial[20];
	uint16_t unused2[3];
	char     firmware[8];
	char     model[40];
	uint16_t sectors_per_int;
	uint16_t unused3;
	uint16_t capabilities[2];
	uint16_t unused4[2];
	uint16_t valid_ext_data;
	uint16_t unused5[5];
	uint16_t size_of_rw_mult;
	uint32_t sectors_28;
	uint16_t unused6[38];
	uint64_t sectors_48;
	uint16_t unused7[152];
} __attribute__((packed)) ata_identify_t;
typedef struct {
	uint8_t reserved;
	uint8_t channel;
	uint8_t drive;
	uint8_t type;
	uint16_t signature;
	uint16_t sup;
	uint32_t cmd_sets;
	uint32_t size;
	char model[41];
	uint16_t base;
	uint16_t ctrl;
	uint16_t nein;
	int slave;
	ata_identify_t identify;
} ata_device_t;
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define ATAPI_CMD_READ       0xA8
#define ATAPI_CMD_EJECT      0x1B

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01
 
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D
static ata_device_t ata_primary_master   = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 0};
static ata_device_t ata_primary_slave    = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 1};
static ata_device_t ata_secondary_master = {.base = 0x170, .ctrl = 0x370, .slave = 0};
static ata_device_t ata_secondary_slave  = {.base = 0x170, .ctrl = 0x370, .slave = 1};
static ata_device_t ata_channel2_master  = {.base = 0x168, .ctrl = 0x360, .slave = 0};
static ata_device_t ata_channel2_slave  = {.base = 0x168, .ctrl = 0x360, .slave = 1};
static ata_device_t ata_channel3_master = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 0};
static ata_device_t ata_channel3_slave = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 1};
void ata_create_device(bool hda,ata_device_t *dev);
void ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
void ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf);
int ata_print_error(ata_device_t *dev);
static char ata_start_char = 'a';
static char ata_cdrom_char = 'a';
static uint64_t next_lba = 0;
void ata_io_wait(ata_device_t *dev) {
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
}
int ata_status_wait(ata_device_t *dev,int timeout) {
	int status;
	if (timeout > 0) {
		int i =0;
		while((status = inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY && (i < timeout))) i++;
	} else {
		while((status = inb(dev->base+ATA_REG_STATUS)) & ATA_SR_BSY);
	}
	return status;	
}
int ata_wait(ata_device_t *dev,int advanced) {
	uint8_t status;
	ata_io_wait(dev);
	status = ata_status_wait(dev,10000); // wait forever!!
	if (advanced) {
		status = inb(dev->base+ATA_REG_STATUS);
		if (status & ATA_SR_ERR) return true;
		if (status & ATA_SR_DF) return true;
		if (!(status & ATA_SR_DRQ)) return true;
	}
	return false;
}
void ata_soft_reset(ata_device_t *dev) {
	outb(dev->ctrl,0x04);
	ata_io_wait(dev);
	outb(dev->ctrl,0x00);
}
int ata_max_offset(ata_device_t *dev) {
	uint64_t sectors = dev->identify.sectors_48;
	if (!sectors) {
		sectors = dev->identify.sectors_28;
	}
	return sectors * 512;
}
// Initialization and detection code
bool ata_device_init(ata_device_t *dev) {
	outb(dev->base+1,1);
	outb(dev->ctrl,0);
	outb(dev->base+ATA_REG_HDDEVSEL, 0xA0 | dev->slave << 4);
	ata_io_wait(dev);
	/*if (inb(dev->base+ATA_REG_STATUS) != 0x50) {
		kprintf("Softreset failed(device not present): %x\n",inb(dev->base+ATA_REG_STATUS));
		return false;
	}*/
	//uint8_t tmpStatus = inb(dev->base+ATA_REG_STATUS);
	outb(dev->base+ATA_REG_LBA0,0);
	outb(dev->base+ATA_REG_LBA1,0);
	outb(dev->base+ATA_REG_LBA2,0);
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_IDENTIFY);
	ata_io_wait(dev);
	//outb(dev->base+ATA_REG_COMMAND,ATA_CMD_CACHE_FLUSH); //test
	if (inb(dev->base+ATA_REG_STATUS) == 0) {
		kprintf("Media not present.\n");
		return false;
	}
	unsigned char status,err,type;
	while(inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY ) {
		/*if ((inb(dev->base+ATA_REG_LBA1) != 0) && (inb(dev->base+ATA_REG_LBA2) != 0)) {
			kprintf("Disk not present or this isn't ATA device\n");
			return false;
		}*/
		// wait for BSY to be cleared 
	}
	// Advanced check for errors
	status = inb(dev->base+ATA_REG_STATUS);
	if (status & ATA_SR_ERR) {
		err = true;
	}
	if (status & ATA_SR_DF) {
		kprintf("Device fault %x\n",status);
		err = true;
	}
	if ((status & ATA_SR_DRQ) == 0) {
		kprintf("Isn't ATA probing ATAPI\n");
		outb(dev->base+ATA_REG_COMMAND,ATA_CMD_IDENTIFY_PACKET);
		ata_io_wait(dev);
		dev->type = IDE_ATAPI;
	}
	if (err) {
		int error;
		if ((error = ata_print_error(dev)) != 0) {
			kprintf("ATA: device error: %x\n",error);
			return false;
		}
	}
	uint16_t *buf = (uint16_t *)&dev->identify;
	for (int i = 0; i < 256; i++) {
		buf[i] = inw(dev->base);
	}
	uint8_t *ptr = (uint8_t *)&dev->identify.model;
	for (int i = 0; i < 39; i+=2) {
		uint8_t tmp = ptr[i+1];
		ptr[i+1] = ptr[i];
		ptr[i] = tmp;
	}
	return true;
}
int ata_device_detect(ata_device_t *dev) {
	ata_soft_reset(dev);
	while(1) {
		uint8_t status = inb(dev->base+ATA_REG_STATUS);
		if (!(status & ATA_SR_BSY)) break;
	}
	if (ata_device_init(dev)) {
		kprintf("ATA name: %s\n",dev->identify.model);
		ata_create_device(true,dev);
	}
	return 0;
}
/* This is a test read and comming soon will be fixed */
void ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf) {
	kprintf("ATA: use readBlock!\n");
	return;
}
/* Only for DEVELOPERS! */
void ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf) {
	// Convert void * to uint8_t *
	uint8_t *target = (uint8_t *)buf;
	uint64_t lba = blockNo;
	uint16_t sectors = how/512;
	if (sectors == 0) sectors = 1;
	ata_device_t *dev = &ata_primary_master;
	outb(dev->base+ATA_REG_HDDEVSEL,0x40);
	outb(dev->base+ATA_REG_SECCOUNT0,(sectors >> 8) & 0xFF);
	outb(dev->base+ATA_REG_LBA0, (lba & 0xff000000) >> 24);
	outb(dev->base+ATA_REG_LBA1, (lba & 0xff000000) >> 32);
	outb(dev->base+ATA_REG_LBA2, (lba & 0xff000000) >> 48);
	outb(dev->base+ATA_REG_SECCOUNT0,sectors & 0xFF);
	outb(dev->base+ATA_REG_LBA0,lba & 0xFF);
	outb(dev->base+ATA_REG_LBA1,(lba >> 8) & 0xFF);
	outb(dev->base+ATA_REG_LBA2,(lba >> 16) & 0xFF);
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_WRITE_PIO_EXT); // ATA_CMD_WRITE_PIO_EXT
	uint8_t i;
	for (i = 0; i < sectors; i++) {
		while(1) {
			uint8_t status = inb(dev->base+ATA_REG_STATUS);
			if (status & ATA_SR_DRQ) {
				// Drive is ready to transfer data
				break;
			} else if (status & ATA_SR_ERR) {
				kprintf("ATA: Disk error. Cancel\n");
				return;
			}
		}
		// Tranfer data over PIO
		for (int u = 0; u < 256; u++) {
			outw(dev->base+ATA_REG_DATA,target[u]);
		}
		target+=256;
	}
	// flush the cache
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_CACHE_FLUSH);
	while(inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY) {}
}
static void ata_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf) {
	ata_device_t *dev = &ata_primary_master;
	if (dev->type == IDE_ATAPI) {
		kprintf("Currently doesn't supported!\n");
		return;
	}
	uint64_t lba = blockNo;
	uint16_t sectors = how/512;
   // kprintf("Read %d sectors at %d\r\n",sectors,lba);
	if (sectors == 0) sectors = 1;
	outb(dev->base+ATA_REG_HDDEVSEL,0x40);
	outb(dev->base+ATA_REG_SECCOUNT0,(sectors >> 8) & 0xFF);
	outb(dev->base+ATA_REG_LBA0,(lba >> 24) & 0xFF);
	outb(dev->base+ATA_REG_LBA1,(lba >> 32) & 0xFF);
	outb(dev->base+ATA_REG_LBA2,(lba >> 40) & 0xFF);
	outb(dev->base+ATA_REG_SECCOUNT0,sectors & 0xFF);
	outb(dev->base+ATA_REG_LBA0,lba & 0xFF);
	outb(dev->base+ATA_REG_LBA1,(lba >> 8) & 0xFF);
	outb(dev->base+ATA_REG_LBA2,(lba >> 16) & 0xFF);
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_READ_PIO_EXT); // ATA_CMD_READ_PIO_EXT
	uint8_t i;
	// convert buffer
	uint8_t *bu = (uint8_t *)buf;
	for (i = 0; i < sectors; i++) {
		while(1) {
			uint8_t status = inb(dev->base+ATA_REG_STATUS);
			if (status & ATA_SR_DRQ) {
				// Disk is ready to transfer data
				break;
			} else if (status & ATA_SR_ERR) {
				kprintf("ATA: Disk error. Cancel\n");
				return;
			}
		}
		insw(dev->base,bu,256);
		bu+=256;
	}
}
void ata_create_device(bool hda,ata_device_t *dev) {
	char *name = kmalloc(4);
	if (hda) {
		name = "hd ";
		name[2] = ata_start_char;
		ata_start_char++;
	} else {
		name = "cd ";
		name[2] = ata_cdrom_char;
		ata_cdrom_char++;
	}
	name[3] = 0;
	dev_t *disk = kmalloc(sizeof(dev_t));
    memset(disk,0,sizeof(dev_t));
	disk->name = name;
	disk->writeBlock = ata_vdev_writeBlock;
	disk->read = ata_vdev_read;
	disk->buffer_sizeMax = 512; // default sector size
	disk->device = dev;
	disk->readBlock = ata_vdev_readBlock;
	// Now register device in our DEVFS
	dev_add(disk);
}
// === Public functions here ===
void atapi_init() {
	kprintf("ATA device driver\n");
	//kprintf("Detecting on channel 0\n");
	ata_device_detect(&ata_primary_master);
	ata_device_detect(&ata_primary_slave);
//	kprintf("Detecting on channel 1\n");
	ata_device_detect(&ata_secondary_master);
	ata_device_detect(&ata_secondary_slave);
	/*kprintf("Detecting on channel 2\n");
	ata_device_detect(&ata_channel2_master);
	ata_device_detect(&ata_channel2_slave);
	kprintf("Detecting on channel 3\n");
	ata_device_detect(&ata_channel3_master);
	ata_device_detect(&ata_channel3_slave);*/
}
// === Loading as module support! ===
static void module_main() {
	atapi_init();
}
int ata_print_error(ata_device_t *dev) {
	uint8_t type = inb(dev->base + ATA_REG_ERROR);
	if (type == 0) {kprintf("No error reported\n"); return type;}
	if (type == 0xff) {kprintf("Media not present but returned error\n"); return type;}
	kprintf("ATA error type: %x\n",type);
	return type;
}
