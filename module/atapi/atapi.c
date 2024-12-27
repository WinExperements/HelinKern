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
#include <arch/mmu.h>
/*
    Ported version of https://github.com/klange/toaruos/blob/toaru-1.x/modules/ata.c
*/
// === Add module name here ===
// === Internal functions here ===
//__attribute__((section(".modname"))) char *name = "atapi";


typedef struct {
	uintptr_t offset;
	uint16_t bytes;
	uint16_t last;
} prdt_t;

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
	unsigned short bmide;
	int slave;
	ata_identify_t identify;
	prdt_t * dma_prdt;
	uintptr_t dma_prdt_phys;
	uint8_t * dma_start;
	uintptr_t dma_start_phys;
	uint32_t bar4;
	uint32_t pci_id;
	int latestSector; // for ata_vdev_read optimization, check latest lba before reading
	int busy; // because we like to not read the PIO register :/
	int ident;
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
#define ATA_CACHE_SIZE 4096
static ata_device_t ata_primary_master   = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 0};
static ata_device_t ata_primary_slave    = {.base = 0x1F0, .ctrl = 0x3F6, .slave = 1};
static ata_device_t ata_secondary_master = {.base = 0x170, .ctrl = 0x370, .slave = 0};
static ata_device_t ata_secondary_slave  = {.base = 0x170, .ctrl = 0x370, .slave = 1};
static ata_device_t ata_channel2_master  = {.base = 0x168, .ctrl = 0x360, .slave = 0};
static ata_device_t ata_channel2_slave  = {.base = 0x168, .ctrl = 0x360, .slave = 1};
static ata_device_t ata_channel3_master = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 0};
static ata_device_t ata_channel3_slave = {.base = 0x1e8, .ctrl = 0x3e0, .slave = 1};
void ata_create_device(bool hda,ata_device_t *dev);
static uint64_t ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static bool ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf);
int ata_print_error(ata_device_t *dev);
static bool ata_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf);
static uint64_t ata_vdev_ioctl(vfs_node_t *node,int request,va_list args);
static char ata_start_char = 'a';
static char ata_cdrom_char = 'a';
static uint64_t next_lba = 0;
static char atapiBuffer[2048];
static char diskBuffer[512];
static ata_device_t *inter_ata;
uintptr_t ata_irq_handler(uintptr_t stack);
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
void ide_write(ata_device_t *dev, unsigned char reg, unsigned char data) {
   if (reg > 0x07 && reg < 0x0C)
      ide_write(dev, ATA_REG_CONTROL, 0x80 | dev->nein);
   if (reg < 0x08)
      outb(dev->base  + reg - 0x00, data);
   else if (reg < 0x0C)
      outb(dev->base  + reg - 0x06, data);
   else if (reg < 0x0E)
      outb(dev->ctrl  + reg - 0x0A, data);
   else if (reg < 0x16)
      outb(dev->bmide + reg - 0x0E, data);
   if (reg > 0x07 && reg < 0x0C)
      ide_write(dev, ATA_REG_CONTROL, dev->nein);
}
// Initialization and detection code
unsigned char ide_read(ata_device_t *dev, unsigned char reg) {
   unsigned char result;
   if (reg > 0x07 && reg < 0x0C)
      ide_write(dev, ATA_REG_CONTROL, 0x80 | dev->nein);
   if (reg < 0x08)
      result = inb(dev->base + reg - 0x00);
   else if (reg < 0x0C)
      result = inb(dev->base  + reg - 0x06);
   else if (reg < 0x0E)
      result = inb(dev->ctrl  + reg - 0x0A);
   else if (reg < 0x16)
      result = inb(dev->bmide + reg - 0x0E);
   if (reg > 0x07 && reg < 0x0C)
      ide_write(dev, ATA_REG_CONTROL, dev->nein);
   return result;
}
bool ata_device_init(ata_device_t *dev,int id,int j) {
	unsigned char status,err = 0;
	int ty = (id == 1 ? 0xB0 : 0xA0);
	outb(dev->base + ATA_REG_HDDEVSEL,ty | (j << 4));
	arch_sti();
	kwait(1);
	arch_cli();
	ide_write(dev,ATA_REG_COMMAND,ATA_CMD_IDENTIFY);
	arch_sti();
	kwait(1);
	arch_cli();
	if (ide_read(dev,ATA_REG_STATUS) == 0) {
		kprintf("Device is empty\r\n");
		return false;
	}
	while(1) {
		status = ide_read(dev,ATA_REG_STATUS);
		if ((status & ATA_SR_ERR)) {
			err = 1;
			break; // maybe ATAPI?
		}
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
	}
	uint8_t ch = ide_read(dev,ATA_REG_LBA1);
	uint8_t cl = ide_read(dev,ATA_REG_LBA2);
	//kprintf("%s: cl -> 0x%x, ch = 0x%x,id -> %d, j -> %d\r\n",__func__,cl,ch,id,j);
	if (cl == 0xEB && ch == 0x14) {
atapiDetect:
		kprintf("ATAPI device detected!\r\n");
		err = 0;
		dev->type = IDE_ATAPI;
		ide_write(dev,ATA_REG_COMMAND,ATA_CMD_IDENTIFY_PACKET);
		ata_io_wait(dev);
		// Repeat process
		while(1) {
			status = ide_read(dev,ATA_REG_STATUS);
			if ((status & ATA_SR_ERR)) {
				kprintf("Device fault fallback!\r\n");
				return false;
			}
			if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
		}
	}
	if (err) {
		return false;
	}
	uint16_t *identf = (uint16_t *)&dev->identify;
	for (int i = 0; i < 256; i++) {
		identf[i] = inw(dev->base);
	}
	uint8_t *ptr = (uint8_t *)&dev->identify.model;
	for (int i = 0; i < 39; i+=2) {
		uint8_t tmp = ptr[i+1];
		ptr[i+1] = ptr[i];
		ptr[i] = tmp;
	}
	kprintf("ATA: Detected device name: %s, type: %s\r\n",dev->identify.model,dev->type == IDE_ATAPI ? "ATAPI" : "IDE");
	ata_create_device(dev->type == IDE_ATA,dev);
	ide_write(dev,ATA_REG_CONTROL,0);
	return true;
}
int ata_device_detect(ata_device_t *dev,bool second) {
	ata_soft_reset(dev);
	ata_io_wait(dev);
	dev->dma_prdt = kmalloc(4096);
	dev->dma_start = kmalloc(8192);
	dev->dma_prdt_phys = (uintptr_t)arch_mmu_getPhysical(dev->dma_prdt);
	dev->dma_start_phys = (uintptr_t)arch_mmu_getPhysical(dev->dma_start);
	dev->dma_prdt[0].offset = dev->dma_start_phys;
	dev->dma_prdt[0].bytes = 16*512;
	dev->dma_prdt[0].last = 0x8000;
	uint16_t command_reg = PciRead32(dev->pci_id, PCI_CONFIG_COMMAND);
	if (!(command_reg & (1 << 2))) {
		command_reg |= (1 << 2); /* bit 2 */
		PciWrite32(dev->pci_id, PCI_CONFIG_COMMAND,command_reg);
		command_reg = PciRead16(dev->pci_id, PCI_CONFIG_COMMAND);
	}
	dev->bar4 =PciRead32(dev->pci_id, PCI_CONFIG_BAR4);
	if (dev->bar4 & 0x00000001) {
		dev->bar4 = dev->bar4 & 0xFFFFFFFC;
	} else {
		kprintf("ERROR: We don't know what to do with this DMA device!\r\n");
		return 1; /* No DMA because we're not sure what to do here */
	}
	for (int i = 0; i < 2; i++) {
		ata_device_init(dev,i,i);
	}

        if (inter_ata == NULL) {
            inter_ata = dev; 
        }
	return 0;
}
/* This function added for EXT2 FS driver */
uint64_t ata_vdev_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buffer) {
   // kprintf("ATA: read %d bytes\n",how);
	ata_device_t *dev = node->device;
	if (dev->type == IDE_ATAPI) {
		kprintf("Device type ATAPI!\r\n");
		return 0;
	}
	int startSec = offset / 512;
	int totalSectors = how / 8192;
	if (totalSectors == 0) {
		ata_vdev_readBlock(node,startSec,512,buffer);
		//kprintf("ATA: copy %d bytes of data\r\n",how);
	//	memcpy(buffer,dev->dma_start,how);
		return how;
	}
	// okay, now we need some loop.
	int total_bytes = how;
	int off = 0;
	while(total_bytes > 0) {
		// We need to check if the buffer can fit into 8192
		int howToCopy = total_bytes - 8192;
		if (howToCopy < 0) {
			// We don't fit, retry
			howToCopy = total_bytes;
		}
		ata_vdev_readBlock(node,startSec,512,buffer);
		total_bytes-=512;
		buffer+=512;
	}
	return how;
}

/* Only for DEVELOPERS! */
bool ata_vdev_writeBlock(struct vfs_node *node,int blockNo,int how,void *buf) {
	// Convert void * to uint8_t *
	uint8_t *target = (uint8_t *)buf;
	uint64_t lba = blockNo;
	uint16_t sectors = how/512;
	if (sectors == 0) sectors = 1;
	ata_device_t *dev = node->device;
	if (dev->type == IDE_ATAPI) return false;
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
				return false;
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
/* Issue detected on real HW:
	If we send the READ command we receive the IRQ, but on some HW
	the reading of status register for device isn't working to
	indicate the process of IRQ for device, as result we got
	the IRQ storm(Dell Latitude E4310).
*/
static bool atapi_readBlock(ata_device_t *dev,int lba,void *buffer) {
	// ATAPI devices use different kind of command(SCSI)
	ide_write(dev,ATA_REG_HDDEVSEL,0xA0 | (dev->slave << 4));
	ata_io_wait(dev);
	ide_write(dev,ATA_REG_FEATURES,0); // PIO.
	int sector_size = 2048;
	ide_write(dev,ATA_REG_LBA1,sector_size & 0xFF);
	ide_write(dev,ATA_REG_LBA2,sector_size >> 8);
	ide_write(dev,ATA_REG_COMMAND,ATA_CMD_PACKET);
	// Prepare SCSI command.
	int timeout = 100;
	while(1) {
		uint8_t status = ide_read(dev,ATA_REG_STATUS);
		if ((status & ATA_SR_ERR)) {
			kprintf("ATAPI early device error. Error: 0x%x\r\n",ide_read(dev,ATA_REG_ERROR));
			return false;
		}
		/*if (timeout-- < 0) {
			kprintf("Device timeout reached\r\n");
			return false;
		}*/
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) break;
	}
	uint8_t *atapiPacket = kmalloc(12);
	memset(atapiPacket,0,12);
	atapiPacket[0] = 0xA8;
	atapiPacket[2] = (lba >> 0x18) & 0xFF;
	atapiPacket[3] = (lba >> 0x10) & 0xFF;
	atapiPacket[4] = (lba >> 0x08) & 0xFF;
	atapiPacket[5] = (lba >> 0x00) & 0xFF;
	atapiPacket[9] = 1;
	uint16_t *packet = (uint16_t *)atapiPacket;
	for (int i = 0; i < 6; i++) {
		// Write command.
		outw(dev->base,packet[i]);
	}
	while(inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY);
	uint16_t *buf = (uint16_t *)buffer;
	for (int i = 0; i < 1024; i++) {
		buf[i] = inw(dev->base);
	}
	kfree(atapiPacket);
	return true;
}
static bool ata_vdev_readBlock(vfs_node_t *node,int lba,int how, void *buf) {
        ata_device_t *dev = node->device;
        if (dev->type == IDE_ATAPI) {
                // We can have different type of address spaces.
                int blocks = (how/2048);
                if (blocks == 0) blocks = 1;
                for (int i = 0,la = lba; i<blocks; i++,la++) {
		              if (!atapi_readBlock(dev,la,buf)) {
                    return false;
                  }
                  buf+=2048;
                }
                return true;
          }
	int numsects = how / 512;
	if (numsects == 0) {
		numsects = 1;
	}
	if (numsects > 256) {
		numsects = 255;
	}
	//ide_write(dev,ATA_REG_CONTROL,0); // enable IRQ for device
	unsigned char lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
   	unsigned char lba_io[6];
   	unsigned int  slavebit      = 0; // Read the Drive [Master/Slave]
   	unsigned int  bus = dev->base; // Bus Base, like 0x1F0 which is also data port.
   	unsigned int  words      = 256; // Almost every ATA drive has a sector-size of 512-byte.
   	unsigned short cyl, i;
   	unsigned char head, sect, err;
	if (lba >= 0x10000000) { // Sure Drive should support LBA in this case, or you are
		// LBA48:
      		lba_mode  = 2;
      		lba_io[0] = (lba & 0x000000FF) >> 0;
      		lba_io[1] = (lba & 0x0000FF00) >> 8;
      		lba_io[2] = (lba & 0x00FF0000) >> 16;
      		lba_io[3] = (lba & 0xFF000000) >> 24;
      		lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
      		lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
      		head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
   	} else if (1)  { // Drive supports LBA?
      		// LBA28:
      		lba_mode  = 1;
      		lba_io[0] = (lba & 0x00000FF) >> 0;
      		lba_io[1] = (lba & 0x000FF00) >> 8;
      		lba_io[2] = (lba & 0x0FF0000) >> 16;
      		lba_io[3] = 0; // These Registers are not used here.
      		lba_io[4] = 0; // These Registers are not used here.
     		 lba_io[5] = 0; // These Registers are not used here.
      		head      = (lba & 0xF000000) >> 24;
   	} else {
      		// CHS:
      		lba_mode  = 0;
      		sect      = (lba % 63) + 1;
      		cyl       = (lba + 1  - sect) / (16 * 63);
      		lba_io[0] = sect;
      		lba_io[1] = (cyl >> 0) & 0xFF;
      		lba_io[2] = (cyl >> 8) & 0xFF;
      		lba_io[3] = 0;
      		lba_io[4] = 0;
      		lba_io[5] = 0;
      		head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
   	}
	while(ide_read(dev,ATA_REG_STATUS) & ATA_SR_BSY);
	if (lba_mode == 0)
      		ide_write(dev, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head); // Drive & CHS.
   	else
      		ide_write(dev, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head); // Drive & LB

   	// (V) Write Parameters;
   	if (lba_mode == 2) {
      		ide_write(dev, ATA_REG_SECCOUNT1,   0);
      		ide_write(dev, ATA_REG_LBA3,   lba_io[3]);
      		ide_write(dev, ATA_REG_LBA4,   lba_io[4]);
      		ide_write(dev, ATA_REG_LBA5,   lba_io[5]);
   	}
   	ide_write(dev, ATA_REG_SECCOUNT0,   numsects);
   	ide_write(dev, ATA_REG_LBA0,   lba_io[0]);
   	ide_write(dev, ATA_REG_LBA1,   lba_io[1]);
   	ide_write(dev, ATA_REG_LBA2,   lba_io[2]);
	cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 2) {
		cmd = ATA_CMD_READ_PIO_EXT;
	}
	arch_cli();
	dev->busy = 1;
	inter_ata = dev;
	outb(dev->base+ATA_REG_COMMAND,cmd);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	inb(dev->base+ATA_REG_ALTSTATUS);
	while(inb(dev->base+ATA_REG_STATUS) & ATA_SR_BSY);
	uint8_t stat = ide_read(dev,ATA_REG_STATUS);
	if (stat & ATA_SR_ERR) {
		kprintf("Device error!\r\n");
		return false;
	}
	if (stat & ATA_SR_DF) {
		kprintf("Device fault!\r\n");
		return false;
	}
	if ((stat & ATA_SR_DRDY) == 0) {
		kprintf("ATA_SR_DRY not set?\r\n");
		return false;
	}
	bool cp = false;
	if (how < 512) {
		cp = true;
	}
	uint8_t *bu = (uint8_t *)cp ? diskBuffer : buf;
	for (int i = 0; i < numsects; i++) {
		insw(dev->base,bu,256);
		bu+=256;
	}
	if (cp) {
		memcpy(buf,diskBuffer,how);
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
	disk->name = strdup(name);
	disk->writeBlock = ata_vdev_writeBlock;
	disk->read = ata_vdev_read;
	disk->buffer_sizeMax = 512; // default sector size
	disk->device = dev;
	disk->readBlock = ata_vdev_readBlock;
	disk->ioctl = ata_vdev_ioctl;
	if (hda) {
		disk->type = DEVFS_TYPE_BLOCK;
	}
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

uintptr_t ata_irq_handler(uintptr_t stack) {
	// ATA specifications
	//kprintf("ATA IRQ! 0x%x 0x%x\r\n",inter_ata->base,inter_ata);
	//uint8_t stat = inb(inter_ata->base+ATA_REG_STATUS);
	ide_read(inter_ata,ATA_REG_ALTSTATUS);
    	inb(inter_ata->base+ATA_REG_STATUS);
	if (inter_ata->busy) {
		inter_ata->busy = 0;
	}
	outb(inter_ata->bar4+4,0x00);
    	//parseATAStatus(stat);
    	/*kprintf("ATA IRQ handler!\n");
    	kprintf("Status of drive(after sending there commands): 0x%x\n",inb(inter_ata->base+ATA_REG_STATUS));
    	kprintf("Error register: 0x%x\n",inb(inter_ata->base+ATA_REG_ERROR));*/
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
    switch((info.classCode << 8) | info.subclass) {
	    case PCI_STORAGE_IDE: {
		    unsigned int BAR0 = PciRead32(id,PCI_CONFIG_BAR0);
		    unsigned int BAR1 = PciRead32(id,PCI_CONFIG_BAR1);
		    unsigned int BAR2 = PciRead32(id,PCI_CONFIG_BAR2);
		    unsigned int BAR3 = PciRead32(id,PCI_CONFIG_BAR3);
		    unsigned int BAR4 = PciRead32(id,PCI_CONFIG_BAR4);
		    uint16_t base = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
		    uint16_t ctrl = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
		    uint16_t sec_base = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
		    uint16_t sec_ctrl = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
		    if (base == 0) {
			    // Use legacy device I/O ports.
			    base = 0x1F0;
			    ctrl = 0x3F6;
			    sec_base = 0x170;
			    sec_ctrl = 0x376;
		    }
		    ata_device_t *ata_pri = kmalloc(sizeof(ata_device_t));
		    ata_pri->base = base;
		    ata_pri->ctrl = ctrl;
		    ata_pri->slave = 0;
		    ata_pri->bar4 = BAR4;
		    ata_pri->bmide = (BAR4 & 0xFFFFFFFC) + 0;
		    ata_pri->pci_id = id;
		    ata_device_t *sec_pri = kmalloc(sizeof(ata_device_t));
		    sec_pri->base = sec_base;
		    sec_pri->ctrl = sec_ctrl;
		    sec_pri->slave = 0;
		    sec_pri->bar4 = BAR4;
		    sec_pri->pci_id = id;
		    sec_pri->bmide = (BAR4 & 0xFFFFFFFC) + 8;
		    inter_ata = ata_pri;
		    uint8_t irqLine = IRQ0 + PciRead8(id,PCI_CONFIG_INTERRUPT_LINE);
		    //kprintf("IRQ for this PCI IDE controller: 0x%x\r\n",irqLine);
		    ata_device_detect(ata_pri,false);
		    ata_device_detect(sec_pri,true);
		    //kprintf("ata: pci: detection finished\n");
		    return true;
    } break;
}
    return false;

}

void atapi_init() {
	kprintf("ATA device driver\n");
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
    interrupt_add(IRQ14,ata_irq_handler);
    interrupt_add(IRQ15,ata_irq_handler);
    
}
// === Loading as module support! ===
int ata_print_error(ata_device_t *dev) {
	uint8_t type = inb(dev->base + ATA_REG_ERROR);
	if (type == 0) {kprintf("No error reported\n"); return type;}
	if (type == 0xff) {kprintf("Media not present but returned error\n"); return type;}
	kprintf("ATA error type: %x\n",type);
	return type;
}

static uint64_t ata_vdev_ioctl(vfs_node_t *node,int request,va_list args) {
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
