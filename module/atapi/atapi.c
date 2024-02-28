#include <output.h>
#include <arch/x86/io.h>
#include <atapi/atapi.h>
#include <dev.h>
#include <mm/alloc.h>
#include <lib/string.h>
/*
    Update 20.01.2024: IDE PCI support
*/
#include <pci/driver.h> // PCI
#include <pci/registry.h>
#include <thread.h>
#include <fs/parttab.h>
#include <kernel.h> // interrupts for da controller
#include <arch/x86/idt.h>
#include <arch.h>
/*
    Ported version of https://github.com/klange/toaruos/blob/toaru-1.x/modules/ata.c
*/
// === Add module name here ===
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
#define ATA_SECTOR_SIZE 512
static ata_device_t ata_primary_master   = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 0};
static ata_device_t ata_primary_slave    = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 1};
static ata_device_t ata_secondary_master = {.base = 0x170, .ctrl = 0x370, .slave = 0};
static ata_device_t ata_secondary_slave  = {.base = 0x170, .ctrl = 0x370, .slave = 1};
static ata_device_t ata_channel2_master  = {.base = 0x168, .ctrl = 0x360, .slave = 0};
static ata_device_t ata_channel2_slave  = {.base = 0x168, .ctrl = 0x360, .slave = 1};
static ata_device_t ata_channel3_master = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 0};
static ata_device_t ata_channel3_slave = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 1};
void ata_create_device(bool hda,ata_device_t *dev);
static int ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static void ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf);
int ata_print_error(ata_device_t *dev);
static void ata_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf);
static int ata_vdev_ioctl(vfs_node_t *node,int request,...);
static char ata_start_char = 'a';
static char ata_cdrom_char = 'a';
static uint64_t next_lba = 0;
static ata_device_t *inter_ata;
void *ata_irq_handler(void *stack);
void ata_io_wait(ata_device_t *dev) {
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
}
static inline void insl(uint16_t port, void *buffer, uint32_t count) {
    asm volatile(
        "cld\n\t"
        "rep insl"
        : "=D" (buffer), "=c" (count)
        : "d" (port), "0" (buffer), "1" (count)
        : "memory"
    );
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
bool ata_device_init(ata_device_t *dev,bool second) {
    arch_sti();
   // kprintf("Interrupts enabled\n");    
  //  kprintf("%s is broken somehow\n",__func__);
   // kprintf("Selecting drive...");
    int bit = (second ? 0xB0 : 0xA0);
	outb(dev->base+ATA_REG_HDDEVSEL, bit | dev->slave << 4);
	//kprintf("ata: waiting for ports\n");
	ata_io_wait(dev);
	/*if (inb(dev->base+ATA_REG_STATUS) != 0x50) {
		kprintf("Softreset failed(device not present): %x\n",inb(dev->base+ATA_REG_STATUS));
		return false;
	}*/
  //  kprintf("selected\n");
	unsigned char cl = inb(dev->base+ATA_REG_LBA1);
	unsigned char ch = inb(dev->base+ATA_REG_LBA2);
	if (cl == 0xFF && ch == 0xFF) {
		kprintf("ata: channel reported no media(0xFF,0xFF)\n");
		return 0;
	}
  //  kprintf("setting ATA_REG_CONTROL to 0...");
    outb(dev->base+ATA_REG_CONTROL,0);
   // kprintf("seted\n");
    //kprintf("getting status....");
	//kprintf("identify command sended, waiting\n");
	uint8_t tmpStatus = inb(dev->base+ATA_REG_STATUS);
  //  kprintf("ok\nwriting some info....");
	outb(dev->base+ATA_REG_LBA0,0);
	outb(dev->base+ATA_REG_LBA1,0);
	outb(dev->base+ATA_REG_LBA2,0);
  //  kprintf("ok\nwriting ATA_CMD_IDENTIFY....");
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_IDENTIFY);
	ata_io_wait(dev);
	//outb(dev->base+ATA_REG_COMMAND,ATA_CMD_CACHE_FLUSH); //test
	if (inb(dev->base+ATA_REG_STATUS) == 0) {
		kprintf("Media not present.\n");
		return false;
	}
   // kprintf("ok\n");
	unsigned char status,err,type;
    //kprintf("ata: waiting to drive become ready\n");
   // kprintf("Waiting device to become ready to transfer data\n");
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
        return false;
	}
	if (status & ATA_SR_DF) {
		kprintf("Device fault %x\n",status);
		err = true;
        return false;
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
  //  kprintf("The device is ready and doesn't reported any errors, reading the identify structure...");
	uint16_t *buf = (uint16_t *)&dev->identify;
	for (int i = 0; i < 256; i++) {
		buf[i] = inw(dev->base);
	}
  //  kprintf("readed\n");
	uint8_t *ptr = (uint8_t *)&dev->identify.model;
	for (int i = 0; i < 39; i+=2) {
		uint8_t tmp = ptr[i+1];
		ptr[i+1] = ptr[i];
		ptr[i] = tmp;
	}
	return true;
}
int ata_device_detect(ata_device_t *dev,bool second) {
	ata_soft_reset(dev);
	ata_io_wait(dev);
	/*while(1) {
		uint8_t status = inb(dev->base+ATA_REG_STATUS);
		if (!(status & ATA_SR_BSY)) break;
	}*/
	if (ata_device_init(dev,second)) {
		kprintf("ATA name: %s\n",dev->identify.model);
		if (dev->identify.model[0] == 0) {
			kprintf("??\r\n");
		} else {
			ata_create_device(true,dev);
		}
        if (inter_ata == NULL) {
            kprintf("Adding inter_ata");
            inter_ata = dev;
            
        }
	}
	return 0;
}
/* This function added for EXT2 FS driver */
int ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buffer) {
   // kprintf("ATA: read %d bytes\n",how);
	int start_block = (offset / 512);
        int end_block = (offset + how -1 ) / 512;
	int b_offset = 0;
	if (offset % 512 || how < 512) {
		unsigned int pr_size = (512 - (offset % 512));
		if (pr_size > how) pr_size = how;
		char *tmp = kmalloc(512);
		ata_vdev_readBlock(node,start_block,512,tmp);
		memcpy(buffer, (void *)((uintptr_t)tmp + ((uintptr_t)offset % 512)), pr_size);
		kfree(tmp);
		b_offset += pr_size;
		start_block++;
	}
	if ((offset + how)  % 512 && start_block <= end_block) {
		int prr_size = (offset + how) % 512;
		char *tmp = kmalloc(512);
                ata_vdev_readBlock(node,end_block,512,tmp);
                memcpy((void *)((uintptr_t)buffer + how - prr_size), tmp, prr_size);
		kfree(tmp);
		end_block--;
	}
	int off = end_block - start_block;
	int readedPerLoop = 0;
	while(start_block <= end_block) {
		ata_vdev_readBlock(node,start_block,512,(void *)buffer + b_offset);
		b_offset+=512;
		start_block++;
		readedPerLoop++;
	}
	return how;
}
/* Only for DEVELOPERS! */
void ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf) {
	// Convert void * to uint8_t *
	uint8_t *target = (uint8_t *)buf;
	uint64_t lba = blockNo;
	uint16_t sectors = how/512;
	if (sectors == 0) sectors = 1;
	ata_device_t *dev = node->device;
	if (dev->type == IDE_ATAPI) return;
	outb(dev->base+ATA_REG_HDDEVSEL,0xE0 | (dev->slave << 4));
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
	//arch_sti();
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
		outsw(dev->base+ATA_REG_DATA,target,256);
        target+=256;
	}
	// flush the cache
	outb(dev->base+ATA_REG_COMMAND,ATA_CMD_CACHE_FLUSH);
	while(inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY) {}
}
// read one sector

static void ata_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf) {
        ata_device_t *dev = node->device;
        if (dev->type == IDE_ATAPI) {
                kprintf("Currently doesn't supported!\n");
                return;
        }
        void *dd = buf;
        int doCopy = false;
        uint64_t lba = blockNo;
        uint16_t sectors = how/512;
        if (sectors == 0) {
                sectors = 1;
                buf = kmalloc(512);
                doCopy = 1;
        }
        //kprintf("Read %d sectors at %d\r\n",sectors,lba);
        if (sectors == 0) sectors = 1;
	if (sectors > 255) {
		kprintf("ATA: Maximum count of sectors that able to read per one command is 255!\n");
		sectors = 254;
	}
        outb(dev->base+ATA_REG_HDDEVSEL,0x40);
        outb(dev->base+ATA_REG_SECCOUNT0,(sectors >> 8) & 0xFF);
		outb(dev->base+ATA_REG_LBA0, (lba & 0xff000000) >> 24);
		outb(dev->base+ATA_REG_LBA1, (lba & 0xff000000) >> 32);
		outb(dev->base+ATA_REG_LBA2, (lba & 0xff000000) >> 48);
		outb(dev->base+ATA_REG_SECCOUNT0,sectors & 0xFF);
		outb(dev->base+ATA_REG_LBA0,lba & 0xFF);
		outb(dev->base+ATA_REG_LBA1,(lba >> 8) & 0xFF);
		outb(dev->base+ATA_REG_LBA2,(lba >> 16) & 0xFF);
        outb(dev->base+ATA_REG_COMMAND,ATA_CMD_READ_PIO_EXT); // ATA_CMD_READ_PIO_EXT
        uint8_t i;
        // convert buffer
        uint8_t *bu = (uint8_t *)buf;
        arch_sti();
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
        if (doCopy) {
                memcpy(dd,buf,how);
                kfree(buf);
        }
		if (sectors > 255) {
			kprintf("ATA: Sectors count are bigger that 0xFF, repeat process\n");
			sectors-=255;
			ata_vdev_readBlock(node,blockNo+255,sectors*512,bu);
		}
}

void ata_create_device(bool hda,ata_device_t *dev) {
	kprintf("ATA: creating device: ctrl -> 0x%x, base -> 0x%x\n",dev->ctrl,dev->base);
	char *name = kmalloc(4);
	if (hda) {
		name = "hd ";
		name[2] = ata_start_char;
		ata_start_char++;
	} else {
		name = "cd ";
		name[2] = ata_cdrom_char;
		ata_cdrom_char++;
		return;
	}
	name[3] = 0;
	dev_t *disk = kmalloc(sizeof(dev_t));
    	memset(disk,0,sizeof(dev_t));
	disk->name = strdup(name);
	disk->writeBlock = ata_vdev_writeBlock;
	disk->read = ata_vdev_read;
	disk->buffer_sizeMax = 512; // default sector size
	disk->device = dev;
	disk->readBlock = ata_vdev_readBlock;
	disk->ioctl = ata_vdev_ioctl;
	kprintf("Device name address: 0x%x\r\n",name);
	if (hda) disk->type = DEVFS_TYPE_BLOCK; // this will trigger the partTab to scan partition table
	// Now register device in our DEVFS
	dev_add(disk);
	kfree(name);
}
// === Public functions here ===

static int irqCounter = 0;

void parseATAStatus(uint8_t status) {
    kprintf("Status Byte: 0x%x\n", status);

    // Bit 7
    if (status & 0x80) {
        kprintf("BSY (Busy) - Device is busy\n");
    } else {
        kprintf("BSY (Busy) - Device is not busy\n");
    }

    // Bit 6
    if (status & 0x40) {
        kprintf("DRDY (Device Ready) - Device is ready\n");
    } else {
        kprintf("DRDY (Device Ready) - Device is not ready\n");
    }

    // Bit 5
    if (status & 0x20) {
        kprintf("DF (Device Fault) - Device fault\n");
    } else {
        kprintf("DF (Device Fault) - No device fault\n");
    }

    // Bit 4
    if (status & 0x10) {
        kprintf("SERV (Service) - Service (command) error\n");
    } else {
        kprintf("SERV (Service) - No service error\n");
    }

    // Bit 3
    if (status & 0x08) {
        kprintf("DRQ (Data Request) - Data is ready to be transferred\n");
    } else {
        kprintf("DRQ (Data Request) - No data ready\n");
    }

    // Bit 2
    if (status & 0x04) {
        kprintf("CORR (Corrected Data) - Data corrected\n");
    } else {
        kprintf("CORR (Corrected Data) - No data correction\n");
    }

    // Bits 1-0
    kprintf("ERR (Error) - Additional error information: 0x%x\n", status & 0x03);
}

void *ata_irq_handler(void *stack) {
    inb(inter_ata->base+ATA_REG_STATUS);
    /*kprintf("ATA IRQ handler!\n");
    kprintf("Status of drive(after sending there commands): 0x%x\n",inb(inter_ata->base+ATA_REG_STATUS));
    kprintf("Error register: 0x%x\n",inb(inter_ata->base+ATA_REG_ERROR));
    kprintf("AA\n");*/
    return stack;
}

static bool PciVisit(unsigned int bus, unsigned int dev, unsigned int func)
{
    unsigned int id = PCI_MAKE_ID(bus, dev, func);

    PciDeviceInfo info;
    info.vendorId = PciRead16(id, PCI_CONFIG_VENDOR_ID);
    if (info.vendorId == 0xffff)
    {
        return false    ;
    }

    info.deviceId = PciRead16(id, PCI_CONFIG_DEVICE_ID);
    info.progIntf = PciRead8(id, PCI_CONFIG_PROG_INTF);
    info.subclass = PciRead8(id, PCI_CONFIG_SUBCLASS);
    info.classCode = PciRead8(id, PCI_CONFIG_CLASS_CODE);
    kprintf("0x%x ",PciRead8(id,PCI_CONFIG_INTERRUPT_LINE));
    switch((info.classCode << 8) | info.subclass) {
	    case PCI_STORAGE_IDE:
		    unsigned int BAR0 = PciRead32(id,PCI_CONFIG_BAR0);
		    unsigned int BAR1 = PciRead32(id,PCI_CONFIG_BAR1);
		    unsigned int BAR2 = PciRead32(id,PCI_CONFIG_BAR2);
		    unsigned int BAR3 = PciRead32(id,PCI_CONFIG_BAR3);
            uint16_t base = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
            uint16_t ctrl = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
            uint16_t sec_base = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
            uint16_t sec_ctrl = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
            ata_device_t *ata_pri = kmalloc(sizeof(ata_device_t));
            ata_pri->base = base;
            ata_pri->ctrl = ctrl;
            ata_pri->slave = 0;
            ata_device_t *ata_sec = kmalloc(sizeof(ata_device_t));
            ata_sec->base = base;
            ata_sec->ctrl = ctrl;
            ata_sec->slave = 1;
            ata_device_t *sec_pri = kmalloc(sizeof(ata_device_t));
            sec_pri->base = sec_base;
            sec_pri->ctrl = sec_ctrl;
            sec_pri->slave = 0;
            ata_device_t *sec_sec = kmalloc(sizeof(ata_device_t));
            sec_sec->base = sec_base;
            sec_sec->ctrl = sec_ctrl;
            sec_sec->slave = 1;
            kprintf("PCI line: 0x%x, 0x%x\n",PciRead8(id,0x3c),PciRead8(id,PCI_CONFIG_INTERRUPT_PIN));
            kprintf("PCI PRIF: 0x%x\n",PciRead8(id,0x09));
            kprintf("ata: pci: detecting drives.... 0x%x\n",base);
            if (base == 0x0) {
                kprintf("device is zero! Skip\n");
                return false;
            }
            inter_ata = &ata_pri;
            interrupt_add(0x2a,ata_irq_handler);
            interrupt_add(0x2f,ata_irq_handler);
            interrupt_add(0x2e,ata_irq_handler);
            ata_device_detect(ata_pri,false);
            ata_device_detect(ata_sec,false);
            ata_device_detect(sec_pri,true);
            ata_device_detect(sec_sec,true);
            //kprintf("ata: pci: detection finished\n");
            return true;
}
    return false;

}

void atapi_init() {
	kprintf("ATA device driver\n");
    kprintf("ata: scanning PCI bus\n");
    for (unsigned int bus = 0; bus < 256; ++bus)
    {
        for (unsigned int dev = 0; dev < 32; ++dev)
        {
            unsigned int baseId = PCI_MAKE_ID(bus, dev, 0);
            uint8_t headerType = PciRead8(baseId, PCI_CONFIG_HEADER_TYPE);
            unsigned int funcCount = headerType & PCI_TYPE_MULTIFUNC ? 8 : 1;

            for (unsigned int func = 0; func < funcCount; ++func)
            {
               // kprintf("ata: pci: visiting PCI\n");
                if (PciVisit(bus, dev, func)) {
                    /*kprintf("Drive detected, but only one controller supported\n");
                    return;*/
                }
                //PciVisit(bus,dev,func);
            }
        }
    }
    
}
// === Loading as module support! ===
int ata_print_error(ata_device_t *dev) {
	uint8_t type = inb(dev->base + ATA_REG_ERROR);
	if (type == 0) {kprintf("No error reported\n"); return type;}
	if (type == 0xff) {kprintf("Media not present but returned error\n"); return type;}
	kprintf("ATA error type: %x\n",type);
	return type;
}

static int ata_vdev_ioctl(vfs_node_t *node,int request,...) {
	ata_device_t *dev = node->device;
	if (dev == NULL || dev->base == 0) return -1;
	switch (request) {
		case 0x10:
			kprintf("ata: spin off drive, ctrl-> 0x%x, base -> 0x%x\n",dev->ctrl,dev->base);
			outb(dev->base+ATA_REG_COMMAND,0xE0);
			while(inb(dev->base + ATA_REG_STATUS) & ATA_SR_BSY);
			break;
		case 0x20:
			kprintf("ata: waking up device\n");
			outb(dev->base+ATA_REG_COMMAND,0xE1);
			while(inb(dev->base + ATA_REG_STATUS) & ATA_SR_BSY);
			break;
		}
	return 0;
}
