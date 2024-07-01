/*
 * AHCI	- Advanced Host Controller Interface driver for HelinKern.
 * Written by WinExperements.
 * Tested on real HW to check OS and driver read/write perfomence.
*/

#include <output.h>
#include <debug.h>
#include <pci/driver.h> // PCI
#include <pci/registry.h>
#include <mm/alloc.h>
#include <arch/mmu.h>
#include <arch.h>
#include <dev.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <atapi/atapi.h>
#include <fs/parttab.h>
#include <arch/x86/idt.h>
#define HBA_PxIS_TFES   (1 << 30)
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX    0x35
#define HBA_PxCMD_CR    (1 << 15)
#define HBA_PxCMD_FR    (1 << 14)
#define HBA_PxCMD_FRE   (1 << 4)
#define HBA_PxCMD_SUD   (1 << 1)
#define HBA_PxCMD_ST    (1 << 0)
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

__attribute__((section(".modname"))) char *name = "ahci";

static int ctrl_id = 0;

typedef enum
{
    FIS_TYPE_REG_H2D    = 0x27, // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34, // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39, // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41, // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46, // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58, // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F, // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1, // Set device bits FIS - device to host
} FIS_TYPE;

typedef struct tagFIS_REG_H2D
{
    // int32_t 0
    uint8_t    fis_type;   // FIS_TYPE_REG_H2D
 
    uint8_t    pmport:4;   // Port multiplier
    uint8_t    rsv0:3;     // Reserved
    uint8_t    c:1;        // 1: Command, 0: Control
 
    uint8_t    command;    // Command register
    uint8_t    featurel;   // Feature register, 7:0
 
    // int32_t 1
    uint8_t    lba0;       // LBA low register, 7:0
    uint8_t    lba1;       // LBA mid register, 15:8
    uint8_t    lba2;       // LBA high register, 23:16
    uint8_t    device;     // Device register
 
    // int32_t 2
    uint8_t    lba3;       // LBA register, 31:24
    uint8_t    lba4;       // LBA register, 39:32
    uint8_t    lba5;       // LBA register, 47:40
    uint8_t    featureh;   // Feature register, 15:8
 
    // int32_t 3
    uint8_t    countl;     // Count register, 7:0
    uint8_t    counth;     // Count register, 15:8
    uint8_t    icc;        // Isochronous command completion
    uint8_t    control;    // Control register
 
    // int32_t 4
    uint8_t    rsv1[4];    // Reserved
} FIS_REG_H2D;

typedef struct tagFIS_REG_D2H
{
    // int32_t 0
    uint8_t    fis_type;    // FIS_TYPE_REG_D2H

    uint8_t    pmport:4;    // Port multiplier
    uint8_t    rsv0:2;      // Reserved
    uint8_t    i:1;         // Interrupt bit
    uint8_t    rsv1:1;      // Reserved

    uint8_t    status;      // Status register
    uint8_t    error;       // Error register

    // int32_t 1
    uint8_t    lba0;        // LBA low register, 7:0
    uint8_t    lba1;        // LBA mid register, 15:8
    uint8_t    lba2;        // LBA high register, 23:16
    uint8_t    device;      // Device register

    // int32_t 2
    uint8_t    lba3;        // LBA register, 31:24
    uint8_t    lba4;        // LBA register, 39:32
    uint8_t    lba5;        // LBA register, 47:40
    uint8_t    rsv2;        // Reserved

    // int32_t 3
    uint8_t    countl;      // Count register, 7:0
    uint8_t    counth;      // Count register, 15:8
    uint8_t    rsv3[2];     // Reserved

    // int32_t 4
    uint8_t    rsv4[4];     // Reserved
} FIS_REG_D2H;

typedef struct tagFIS_DATA
{
    // int32_t 0
    uint8_t    fis_type;   // FIS_TYPE_DATA
 
    uint8_t    pmport:4;   // Port multiplier
    uint8_t    rsv0:4;     // Reserved
 
    uint8_t    rsv1[2];    // Reserved
 
    // int32_t 1 ~ N
    uint32_t   data[1];    // Payload
} FIS_DATA;

typedef volatile struct tagHBA_PORT
{
    uint32_t   clb;        // 0x00, command list base address, 1K-byte aligned
    uint32_t   clbu;       // 0x04, command list base address upper 32 bits
    uint32_t   fb;     // 0x08, FIS base address, 256-byte aligned
    uint32_t   fbu;        // 0x0C, FIS base address upper 32 bits
    uint32_t   is;     // 0x10, interrupt status
    uint32_t   ie;     // 0x14, interrupt enable
    uint32_t   cmd;        // 0x18, command and status
    uint32_t   rsv0;       // 0x1C, Reserved
    uint32_t   tfd;        // 0x20, task file data
    uint32_t   sig;        // 0x24, signature
    uint32_t   ssts;       // 0x28, SATA status (SCR0:SStatus)
    uint32_t   sctl;       // 0x2C, SATA control (SCR2:SControl)
    uint32_t   serr;       // 0x30, SATA error (SCR1:SError)
    uint32_t   sact;       // 0x34, SATA active (SCR3:SActive)
    uint32_t   ci;     // 0x38, command issue
    uint32_t   sntf;       // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t   fbs;        // 0x40, FIS-based switch control
    uint32_t   rsv1[11];   // 0x44 ~ 0x6F, Reserved
    uint32_t   vendor[4];  // 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct tagHBA_MEM
{
    // 0x00 - 0x2B, Generic Host Control
    uint32_t   cap;        // 0x00, Host capability
    uint32_t   ghc;        // 0x04, Global host control
    uint32_t   is;     // 0x08, Interrupt status
    uint32_t   pi;     // 0x0C, Port implemented
    uint32_t   vs;     // 0x10, Version
    uint32_t   ccc_ctl;    // 0x14, Command completion coalescing control
    uint32_t   ccc_pts;    // 0x18, Command completion coalescing ports
    uint32_t   em_loc;     // 0x1C, Enclosure management location
    uint32_t   em_ctl;     // 0x20, Enclosure management control
    uint32_t   cap2;       // 0x24, Host capabilities extended
    uint32_t   bohc;       // 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    uint8_t    rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t    vendor[0x100-0xA0];

    // 0x100 - 0x10FF, Port control registers
    HBA_PORT    ports[1];   // 1 ~ 32
} HBA_MEM;

typedef struct tagHBA_PRDT_ENTRY
{
    uint32_t   dba;        // Data base address
    uint32_t   dbau;       // Data base address upper 32 bits
    uint32_t   rsv0;       // Reserved

    // DW3
    uint32_t   dbc:22;     // Byte count, 4M max
    uint32_t   rsv1:9;     // Reserved
    uint32_t   i:1;        // Interrupt on completion
} HBA_PRDT_ENTRY;

typedef struct tagHBA_CMD_TBL
{
    // 0x00
    uint8_t    cfis[64];   // Command FIS

    // 0x40
    uint8_t    acmd[16];   // ATAPI command, 12 or 16 bytes

    // 0x50
    uint8_t    rsv[48];    // Reserved

    // 0x80
    HBA_PRDT_ENTRY  prdt_entry[1];  // Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;

typedef struct tagHBA_CMD_HEADER
{
    // DW0
    uint8_t    cfl:5;      // Command FIS length in DWORDS, 2 ~ 16
    uint8_t    a:1;        // ATAPI
    uint8_t    w:1;        // Write, 1: H2D, 0: D2H
    uint8_t    p:1;        // Prefetchable

    uint8_t    r:1;        // Reset
    uint8_t    b:1;        // BIST
    uint8_t    c:1;        // Clear busy upon R_OK
    uint8_t    rsv0:1;     // Reserved
    uint8_t    pmp:4;      // Port multiplier port

    uint16_t    prdtl;      // Physical region descriptor table length in entries

    // DW1
    volatile
    uint32_t   prdbc;      // Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t   ctba;       // Command table descriptor base address
    uint32_t   ctbau;      // Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t   rsv1[4];    // Reserved
} __attribute__((packed)) HBA_CMD_HEADER;

struct sata_info {
	void *clb;
	void *fb;
	void *ctba[32];
	HBA_PORT *port;
	HBA_MEM *abar;
	int type; // SATAPI,SATA
	int sector_size;
	int media_present; // For SATAPI
	int portID;
    	int devID;
};

struct glob {
    HBA_MEM *regs;
    int pi;
    HBA_PORT *ports[32];
    int waitForIRQ[32]; // We are waiting for IRQ :)
};

#define SATA_SIG_ATA    0x00000101  // SATA drive
#define SATA_SIG_ATAPI  0xEB140101  // SATAPI drive
#define SATA_SIG_SEMB   0xC33C0101  // Enclosure management bridge
#define SATA_SIG_PM 0x96690101  // Port multiplier
#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SATAPI 4
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define HBA_PORT_DET_PRESENT 3
#define HBA_PORT_IPM_ACTIVE 1

enum {
	GHC_AE			= (1U << 31),	// AHCI Enable
	GHC_MRSM		= (1 << 2),		// MSI Revert to Single Message
	GHC_IE			= (1 << 1),		// Interrupt Enable
	GHC_HR			= (1 << 0),		// HBA Reset **RW1**
};

enum {
	CAP_S64A		= (1U << 31),	// Supports 64-bit Addressing
	CAP_SNCQ		= (1 << 30),	// Supports Native Command Queuing
	CAP_SSNTF		= (1 << 29),	// Supports SNotification Register
	CAP_SMPS		= (1 << 28),	// Supports Mechanical Presence Switch
	CAP_SSS			= (1 << 27),	// Supports Staggered Spin-up
	CAP_SALP		= (1 << 26),	// Supports Aggressive Link Power Management
	CAP_SAL			= (1 << 25),	// Supports Activity LED
	CAP_SCLO		= (1 << 24),	// Supports Command List Override
	CAP_ISS_MASK	= 0xf,			// Interface Speed Support
	CAP_ISS_SHIFT	= 20,
	CAP_SNZO		= (1 << 19),	// Supports Non-Zero DMA Offsets
	CAP_SAM			= (1 << 18),	// Supports AHCI mode only
	CAP_SPM			= (1 << 17),	// Supports Port Multiplier
	CAP_FBSS		= (1 << 16),	// FIS-based Switching Supported
	CAP_PMD			= (1 << 15),	// PIO Multiple DRQ Block
	CAP_SSC			= (1 << 14),	// Slumber State Capable
	CAP_PSC			= (1 << 13),	// Partial State Capable
	CAP_NCS_MASK	= 0x1f,			// Number of Command Slots
									// (zero-based number)
	CAP_NCS_SHIFT	= 8,
	CAP_CCCS		= (1 << 7),		// Command Completion Coalescing Supported
	CAP_EMS			= (1 << 6),		// Enclosure Management Supported
	CAP_SXS			= (1 << 5),		// Supports External SATA
	CAP_NP_MASK		= 0x1f,			// Number of Ports (zero-based number)
	CAP_NP_SHIFT	= 0,
};
typedef union _ahci_sact {
	// ReactOS source.
	struct {
		unsigned long DET : 4;
		unsigned long SPD : 4;
		unsigned long IPM : 4;
		unsigned long SPM : 4;
		unsigned long PMP : 4;
		unsigned long reserved : 12;
	};
	unsigned long status;
} ahci_sact;
#define SSTS_PORT_DET_NODEV		0x00000000	// no device detected
#define SSTS_PORT_DET_NOPHY		0x00000001	// device present but PHY not est.
#define SSTS_PORT_DET_PRESENT	0x00000003	// device present and PHY est.
#define SSTS_PORT_DET_OFFLINE	0x00000004	// device offline due to disabled
#define SCTL_PORT_DET_NOINIT	0x00000000	// no initalization request
#define SCTL_PORT_DET_INIT		0x00000001	// perform interface initalization
#define SCTL_PORT_DET_DISABLE	0x00000004	// disable phy
#define HBA_PORT_DET_MASK		0x0000000f	// Device Detection
#define HBA_PORT_IPM_MASK		0x00000f00	// Interface Power Management
#define SSTS_PORT_IPM_ACTIVE	0x00000100	// active state
#define SSTS_PORT_IPM_PARTIAL	0x00000200	// partial state
#define SSTS_PORT_IPM_SLUMBER	0x00000600	// slumber power management state
#define SSTS_PORT_IPM_DEVSLEEP	0x00000800	// devsleep power management state
#define SCTL_PORT_IPM_NORES		0x00000000	// no power restrictions
#define SCTL_PORT_IPM_NOPART	0x00000100	// no transitions to partial
#define SCTL_PORT_IPM_NOSLUM	0x00000200	// no transitions to slumber
#define htonl(l)  ( (((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
static HBA_MEM *ctrl_data;
static PciBar *bar;
static int controllerID;

static int find_cmdslot(HBA_PORT *port,HBA_MEM *abar);
// Read or write to SATA device.
static uint8_t rw_sata(struct sata_info *pdata, uint64_t lba, uint32_t count,char *buf,bool waitForComplete,bool write);
static bool ahci_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf);
static bool ahci_vdev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf);
static uint64_t ahci_vdev_write(vfs_node_t *node,uint64_t blockNo,uint64_t how, void *buf);
void registerDev(struct sata_info *inf);
void* align_addr(void* address,int al);
void *ahci_handler(void *regs);
char datBuff[16384] __attribute__((aligned(1024)));
bool identifyDisk(HBA_PORT *port,HBA_MEM *abar,int type);
int AHCI_BASE;
static struct glob *gl_dat;
static void reset_controller();
static void reset_port(HBA_PORT *port);
// Sends non-data command to target port using device type
//	     Send the non-data command to device				if device isn't ATAPI this parameter can
//	     									be NULL.
//	     										   Buffer is used for non read
//	     										   commands(use read_sata instead)
static void sendCommand(int cmd,HBA_PORT *port,HBA_MEM *mem,int type,uint8_t *atapiPacket,void *buffer,int buffSize);
int ahciVendor;
uint64_t ahci_vdev_ioctl(vfs_node_t *node,int request,va_list arg);
static char satapi_char = '0';
static char sata_char = '0';
static void ahci_mapPort(HBA_PORT *port);
static bool ahci_vdev_poweroff(vfs_node_t *node);
static bool ahci_vdev_poweron(vfs_node_t *node);
static bool setupAtapi(HBA_PORT *port,HBA_MEM *abar);
static bool sendATAPICommand(HBA_PORT *port,uint8_t *cmd,int size,void *buffer);
#define AHCI_CMD_SPIN_UP	0x02
#define AHCI_CMD_POWER_ON	0x04
static void PciVisit(unsigned int bus, unsigned int dev, unsigned int func)
{
    unsigned int id = PCI_MAKE_ID(bus, dev, func);

    PciDeviceInfo info;
    info.vendorId = PciRead16(id, PCI_CONFIG_VENDOR_ID);
    if (info.vendorId == 0xffff)
    {
        return;
    }

    info.deviceId = PciRead16(id, PCI_CONFIG_DEVICE_ID);
    info.progIntf = PciRead8(id, PCI_CONFIG_PROG_INTF);
    info.subclass = PciRead8(id, PCI_CONFIG_SUBCLASS);
    info.classCode = PciRead8(id, PCI_CONFIG_CLASS_CODE);
    if (((info.classCode << 8) | info.subclass) == PCI_STORAGE_SATA) {
        int addr = PciRead32(id,PCI_CONFIG_BAR5);
	    PciWrite16(id,PCI_CONFIG_COMMAND,PciRead16(id,PCI_CONFIG_COMMAND) | 0x0002 | 0x0004);
	    ctrl_data = (HBA_MEM *)(addr & ~0xf);
        // Map the space
        int pages = (sizeof(HBA_MEM))+1;
        for (int page = 0; page < pages; page++) {
            int align = page*4096;
            arch_mmu_mapPage(NULL,addr+align,addr+align,7);
        }
        ctrl_id = id;
      controllerID = id;
      ahciVendor = info.vendorId;
    }
}

static int check_type(HBA_PORT *port) {
	uint16_t ssts = port->ssts;
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;
	/*
	 * Older version of this driver expects that the Firmware doesn't break the physical
	 * connection between controller and port. But when i testing this driver on
	 * Lenovo ThinkPad T400 i understand that we need to handle situation when
	 * the FW breaks the connection before kernel/driver boots. Also this
	 * will help us to support hot swap support on target systems.
	*/
	if ((port->sig >> 24) == 0xff) {
		// Some HW can report no phys link but can report the device signature.
		return AHCI_DEV_NULL;
	}
	switch(port->sig) {
		case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
		case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
		case SATA_SIG_PM:
		return AHCI_DEV_PM;
		default:
		return AHCI_DEV_SATA;
	}
	return 0;
}

bool start_cmd(HBA_PORT *port)
{
    // Set FRE (bit4) and ST (bit0)
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR);
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
    return true;
}

// Stop command engine
void stop_cmd(HBA_PORT *port)
{
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    // Wait until FR (bit14), CR (bit15) are cleared
    while(1)
    {
        //printk("cmd: %d\n", port->cmd);
        if (port->cmd & HBA_PxCMD_FR)
            continue;
        if (port->cmd & HBA_PxCMD_CR)
            continue;
        break;
    }
}

void* align_address(void* address, size_t alignment) {
    uintptr_t addr = (uintptr_t)address;
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned_addr;
}

// Our hell begins here
static void rebase_port(HBA_PORT *port, int portno,struct sata_info *info) {
    // Reset controller and it's ports
    uint8_t ipm = (port->ssts >> 8) & 0x0F;
    if (false) {
    	    stop_cmd(port);
    } else {
	    kprintf("AHCI: Stopping port command engine\r\n");
	    stop_cmd(port);
	    if (port->cmd & HBA_PxCMD_ST) {
		    kprintf("Port is ACTIVE!\r\n");
		    return;
	    }
	    kprintf("AHCI: Perfoming interface COMRESET\r\n");
	    // We need IRQ.
	    kprintf("DET before reset: %d\r\n",port->ssts);
	    clock_setShedulerEnabled(false);
	    arch_sti();
	    port->cmd |= (1<<1);
	    kwait(5);
	    port->sctl &= ~(1<<1);
	    kprintf("AHCI: Waiting to port become online, %d\r\n",port->ssts);
	    while(1) {
		    uint8_t det = port->ssts & 0x0F;
		    if (det == 3) {
			    kprintf("Communication successfull. det = 0%x\r\n",det);
			    break;
			} else {
				kprintf("Port SSTS: %d\r\n",det);
			}
		}
    }
    port->serr =1;//For each implemented port, clear the PxSERR register, by writing 1 to each mplemented location
    port->is=0;//
    port->ie = 0;
    port->clb = AHCI_BASE + (portno<<10);
    port->clbu = 0;
    memset((void*)(port->clb), 0, 1024);

    info->clb = (void *)port->clb;
	// FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	port->fb = AHCI_BASE + (32<<10) + (portno<<8);
	port->fbu = 0;
	memset((void*)(port->fb), 0, 256);
    info->fb = (void *)port->fb;
	// Command table offset: 40K + 8K*portno
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(port->clb);
	for (int i=0; i<32; i++)
	{
		cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
					// 256 bytes per command table, 64+16+48+16*8
		// Command table offset: 40K + 8K*portno + cmdheader_index*256
		cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
		cmdheader[i].ctbau = 0;
		memset((void*)cmdheader[i].ctba, 0, 256);
	}
	port->cmd |= (1<<28);
	kprintf("AHCI: Starting command engine\r\n");
    	start_cmd(port);
    port->is = 0;   
    port->ie = 0xffffffff;
}

void ahci_init() {
    clock_setShedulerEnabled(false);
    arch_sti();
    int pages = 4095;
    AHCI_BASE = alloc_getPage();
    for (int i = 0; i < pages; i++) {
	alloc_getPage();
    }
    // arch_mmu_mapPage can allocate new PT, so we need to make 2 different loops.
    for (int i = 0; i < pages; i++) {
	int addr = AHCI_BASE + (i * 4096);
	arch_mmu_mapPage(NULL,addr,addr,3);
    }
    memset(datBuff,0,512);
    void *a = arch_mmu_getAspace();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    // Scan each BUS to find the SATA controller
    for (unsigned int bus = 0; bus < 256; ++bus)
    {
        for (unsigned int dev = 0; dev < 32; ++dev)
        {
            unsigned int baseId = PCI_MAKE_ID(bus, dev, 0);
            uint8_t headerType = PciRead8(baseId, PCI_CONFIG_HEADER_TYPE);
            unsigned int funcCount = headerType & PCI_TYPE_MULTIFUNC ? 8 : 1;

            for (unsigned int func = 0; func < funcCount; ++func)
            {
                PciVisit(bus, dev, func);
            }
        }
    }
    if (ctrl_id == 0) {
        return;
    }
    int pi = ctrl_data->pi;
    int i =0;
    struct sata_info *sata = NULL;
    gl_dat = (struct glob *)kmalloc(sizeof(struct glob));
    memset(gl_dat,0,sizeof(struct glob));
    if (!(ctrl_data->bohc & 2)) {
            //kprintf("Requesting ownership for port %d\r\n",i);
            ctrl_data->bohc = (ctrl_data->bohc &~8) | 2;
            int endtime = clock_getUptimeMsec()+100;
            while((ctrl_data->bohc & 1) && clock_getUptimeMsec() < endtime);
            if ((ctrl_data->bohc & 1)) {
                //kprintf("BIOS! It's MY drive, i'll get it force!\r\n");
                ctrl_data->bohc = 2;
                ctrl_data->bohc |= 8;
            } else {
                //kprintf("Thank you, BIOS for the permission!\r\n");
            }
        } else {
            kprintf("Oh! The firmware is automatically switch the drive into OS mode, i like this!\r\n");
        }
	for (i = 0; i < 5; i++) {
		ctrl_data->ghc |= 0x80000000;
		asm volatile("hlt"); // wait for any interrupt
		if (ctrl_data->ghc & 0x80000000) {
			break;
		}
	}
	if (i == 5) {
		kprintf("AHCI: ERROR: Failed to switch AHCI controller into operating state!\r\n");
		return;
	}
	for (i = 0; i < 5; i++) {
		ctrl_data->ghc |= 0x80000000;
		asm volatile("hlt");
		if (ctrl_data->ghc & 0x80000000) {
			break;
		}
	}
	if (i == 5) {
		kprintf("AHCI: ERROR: Failed to switch AHCI controller into operating state(STAGE 2)\r\n");
		return;
	}
	uint8_t interPin = PciRead8(ctrl_id,PCI_CONFIG_INTERRUPT_LINE);
	kprintf("AHCI: Detected PCI interrupt PIN: 0x%x\r\n",IRQ0 + interPin);
        interrupt_add(IRQ0 + interPin,ahci_handler);
	ctrl_data->ghc |= GHC_IE;
	clock_setShedulerEnabled(true);
    	while(i < 32) {
	if (pi & 1) {
        	HBA_PORT* port = (HBA_PORT*)&ctrl_data->ports[i];
		int type = check_type(port);
		if (type == AHCI_DEV_SATA) {
			sata = kmalloc(sizeof(struct sata_info));
			memset(sata,0,sizeof(struct sata_info));
			sata->abar = ctrl_data;
            		sata->devID = controllerID;
			sata->portID = i;
			kprintf("AHCI: Rebasing port\r\n");
			gl_dat->ports[i] = port;
			gl_dat->pi = pi;
			rebase_port(port,i,sata);
            		sata->port = port;
            		// ahaha, map the memory for port
            		ahci_mapPort(port);
			if (!identifyDisk(port,ctrl_data,AHCI_DEV_SATA)) {
			}
			sata->type = type;
			// IDK why.
			registerDev(sata);
		} else if (type == AHCI_DEV_SATAPI) {
           		 kprintf("Found CDROM drive on port %d\r\n",i);
			 sata = kmalloc(sizeof(struct sata_info));
			 memset(sata,0,sizeof(struct sata_info));
			 sata->abar = ctrl_data;
			 sata->devID = controllerID;
			 sata->portID = i;
			 gl_dat->ports[i] = port;
			 gl_dat->pi = pi;
			 rebase_port(port,i,sata);
			 int ledEnb = (1<<25);
			 port->cmd &= ~ledEnb;
			 sata->port = port;
			 ahci_mapPort(port);
			 sendCommand(0x1a,port,ctrl_data,AHCI_DEV_SATAPI,NULL,NULL,0);
			 /*if (!identifyDisk(port,ctrl_data,AHCI_DEV_SATAPI)) {
				 kprintf("Whahahaha\r\n");
			 }*/
			 //sendCommand(0x25,port,ctrl_data,type,NULL,NULL,0);
			 sata->type = type;
			 registerDev(sata);
        	} else if (type == AHCI_DEV_NULL) {
            		pi >>= 1;
			i++;
			continue;
        	}
	}
piCont:
	pi >>= 1;
	i++;
    }
   arch_mmu_switch(a);
    kprintf("AHCI driver done\n");
}
static int find_cmdslot(HBA_PORT *port,HBA_MEM *abar) {
    uint32_t slots = (port->sact | port->ci);
    int cmdslots = (abar -> cap & 0x0f00) >> 8;
    for (int i=0; i<cmdslots; i++)
    {
        if ((slots&1) == 0)
            return i;
        slots >>= 1;
    }
    return -1;
}

void* align_addr(void* address,int al) {
    uintptr_t addr = (uintptr_t)address;
    al--;
    uintptr_t aligned_addr = (addr + al) & ~al;
    return (void*)aligned_addr;
}
bool identifyDisk(HBA_PORT *port,HBA_MEM *abar,int type) {
	// clean any interrupts before seting up new command.
	port->is = (uint32_t)-1;
	int spin; // port hung detection.
	int cmdslot = find_cmdslot(port,abar);
	if (cmdslot == -1) {
		// very strange.
		return false;
	}
	HBA_CMD_HEADER *cmd = (HBA_CMD_HEADER *)port->clb;
	cmd+=cmdslot;
	cmd->cfl =  sizeof(FIS_REG_H2D)/sizeof(uint32_t);
	//if (type == AHCI_DEV_SATAPI) cmd->a = 1;
	cmd->p = 0;
	cmd->prdtl = 1;
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(cmd->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
 		(cmd->prdtl-1)*sizeof(HBA_PRDT_ENTRY));
	cmdtbl->prdt_entry[0].dba = (uint32_t)datBuff;
	cmdtbl->prdt_entry[0].dbc = sizeof(ata_identify_t);
	cmdtbl->prdt_entry[0].i = 0;
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
	//memset(cmdfis,0,sizeof(FIS_REG_H2D));
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->command = (type == AHCI_DEV_SATA ? ATA_CMD_IDENTIFY : ATA_CMD_IDENTIFY_PACKET);
	cmdfis->c = 1;
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	if (spin == 1000000)
	{
		kprintf("Port is hung, tfd: 0x%x, serr: 0x%x, ssst: 0x%x, cmd: 0x%x\n",port->tfd,port->serr,port->ssts,port->cmd);
		return false;
	}
	port->ci = 1<<cmdslot;
	// Wait for the command to be processed.
	while(1) {
		if ((port->ci & (1<<cmdslot)) == 0) {
			break;
		}
		if (port->is & HBA_PxIS_TFES) {
			kprintf("AHCI: Identify command cause device task fault.\r\n");
			port->serr = port->serr;
			stop_cmd(port);
			port->is = (uint32_t)-1;
			port->ie = 0;
			start_cmd(port);
			port->ie = 0xffffff;
			return false;
		}
	}
	ata_identify_t *ident = (ata_identify_t *)datBuff;
	ata_convertIdentify(ident);
	kprintf("Device name: %s, size %d MB\r\n",ident->model,type == AHCI_DEV_SATA ? (ident->sectors_48 * 512) / 1024 / 1024 : 0);
	return true;
}

static uint8_t rw_sata(struct sata_info *pdata, uint64_t lba, uint32_t count,char *bufa,bool waitForEnd,bool write) {
	HBA_PORT *port = pdata->port;
    	/* After probe and errors, the buffer must be one kilobyte aligned. */
	void *buf = bufa; // We support reading of 16KB per one call.
    	port->is = (uint32_t) -1;		// Clear pending interrupt bits
	port->serr = port->serr;
    	//kprintf("AHCI: [DEBUG]: Pre FIS: TFD: 0x%x\n",port->tfd);
	int spin = 0; // Spin lock timeout counter
	int slot = find_cmdslot(port,pdata->abar);
	if (slot == -1)
		return false;
 
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
	cmdheader->w = write;		// Read from device
    	cmdheader->c = 0;               // Read from device
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;	// PRDT entries count
 
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(cmdheader->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
 		(cmdheader->prdtl-1)*sizeof(HBA_PRDT_ENTRY));
 
	// 8K bytes (16 sectors) per PRDT
    	int i = 0;
	for (i=0; i<cmdheader->prdtl-1; i++)
	{
		cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
		cmdtbl->prdt_entry[i].dbc = 8*1024-1;	// 8K bytes (this value should always be set to 1 less than the actual value)
		cmdtbl->prdt_entry[i].i = 1;
		buf += 4*1024;	// 4K words
		count -= 16;	// 16 sectors
	}
	// Last entry
	cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
	if (pdata->type == AHCI_DEV_SATAPI) {
		cmdtbl->prdt_entry[i].dbc = (count * 2048) -1;
	} else {
		cmdtbl->prdt_entry[i].dbc = (count<<9)-1;	// 512 bytes per sector
	}
	cmdtbl->prdt_entry[i].i = 1;
	// Setup command
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
 
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;	// Command
	if (pdata->type == AHCI_DEV_SATAPI) {
		cmdfis->command = 0xA0; // ATAPI packet.
		cmdfis->featurel = 5;
	} else {
		cmdfis->command = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EX;
	}
	if (pdata->type == AHCI_DEV_SATA) {
		cmdfis->lba0 = (uint8_t)lba & 0xff;
		cmdfis->lba1 = (uint8_t)(lba >> 8) & 0xff;
		cmdfis->lba2 = (uint8_t)(lba >> 16) & 0xff;
		cmdfis->device = 1<<6;	// LBA mode 
		cmdfis->lba3 = (uint8_t)(lba >> 24) & 0xff;
		cmdfis->lba4 = (uint8_t)(lba >> 32) & 0xff;
		cmdfis->lba5 = (uint8_t)(lba >> 40) & 0xff;
		cmdfis->countl = count & 0xFF;
		cmdfis->counth = (count >> 8) & 0xFF;
	} else if (pdata->type = AHCI_DEV_SATAPI) {
		cmdheader->a = 1;
		memset(cmdtbl->acmd,0,16);
		cmdtbl->acmd[0] = 0xA8;
		cmdtbl->acmd[2] = (uint8_t)((lba >> 24) & 0xff);
		cmdtbl->acmd[3] = (uint8_t)((lba >> 16) & 0xff);
		cmdtbl->acmd[4] = (uint8_t)((lba >> 8) & 0xff);
		cmdtbl->acmd[5] = (uint8_t)((lba >> 0) & 0xff);
		cmdtbl->acmd[9] = (uint8_t)(count & 0xff);
	}
	// The below loop waits until the port is no longer busy before issuing a new command
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	if (spin == 1000000)
	{
		kprintf("Port is hung, tfd: 0x%x, serr: 0x%x, ssst: 0x%x, cmd: 0x%x\n",port->tfd,port->serr,port->ssts,port->cmd);
		return false;
	}
	gl_dat->waitForIRQ[pdata->portID] = true;
	port->ci = 1<<slot;	// Issue command
	if (!waitForEnd) {
		// we don't actually care(EXPEREMENT)
		return true;
	}
	// Wait for completion
	while(gl_dat->waitForIRQ[pdata->portID]);
	// Okay, we handled IRQ.
	/*while (1)
	{
		// In some longer duration reads, it may be helpful to spin on the DPS bit 
		// in the PxIS port field as well (1 << 5)
		if ((port->ci & (1<<slot)) == 0) 
			break;
		if (port->is & HBA_PxIS_TFES)	// Task file error
		{
			kprintf("Read disk erro(first) at sector %d with total count of %d sectors\r\n",lba,count);
			port->serr = port->serr;
			// We need to reset port.
			stop_cmd(port);
			port->is = -1;
			port->ie = 0;
			start_cmd(port);
			port->ie = 0xffffffff;
			return false;
		}
	}*/
	// Check again
	if (port->is & HBA_PxIS_TFES)
	{
		kprintf("Read disk error(second): at sector %d with total count of %d sectors\n",lba,count);
		port->serr = port->serr;
		stop_cmd(port);
		port->is = -1;
		port->ie = 0;
		start_cmd(port);
		port->ie = 0xffffffff;
		return false;
	}
	return true;
}

static bool ahci_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf) {
    // Convert blockNo to the startl and starth
	// Yes.
	struct sata_info *inf = (struct sata_info *)node->device;
	int block_size = inf->type == AHCI_DEV_SATA ? 512 : 2048;
	if (blockNo < 0) return false;
	// Here by some reason the interrupts begin disabled.
	// Make sure we working in kernel space.
	void *curSpace = arch_mmu_getAspace();
	process_t *prc = thread_getThread(thread_getCurrent());
	void *prcSpace = prc->aspace;
	void *switchTo = arch_mmu_getKernelSpace();
	prc->aspace = switchTo;
	arch_mmu_switch(switchTo);
	bool ret = false;
	arch_sti();
	for (int i = 0; i < 1; i++) {
    		int blocks = how / block_size;
    		if (blocks == 0) {
        		blocks = 1;
    		}
    		if (!rw_sata(inf,blockNo,blocks,datBuff,true,false)) {
			goto clean;
		}
    		memcpy(buf,datBuff,blocks*block_size);
	}
	ret = true;
clean:
	// Switch back.
	arch_mmu_switch(curSpace);
	prc->aspace = prcSpace;
	return ret;
}

static uint64_t ahci_vdev_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buffer) {
    struct sata_info *inf = (struct sata_info *)node->device;
    if (inf->type == AHCI_DEV_SATAPI) return 0;
    bool reqSwitch = (arch_mmu_getAspace() == arch_mmu_getKernelSpace() ? false : true);
    process_t *prc = thread_getThread(thread_getCurrent());
    void *curSpace = arch_mmu_getAspace();
    void *orig = prc->aspace;
    void *prcOrig = prc->aspace;
    bool curProcIsnt = false;
    if (prc->pid == 0 || prc->aspace != curSpace) {
	    orig = curSpace;
	    curProcIsnt = true;
    }
    prc->aspace = arch_mmu_getKernelSpace();
    arch_mmu_switch(prc->aspace);
    int remain = how % 512;
    int realSize = how - remain;
    int blocks = realSize / 512;
    int realOffset = offset % 512;
    //kprintf("%s: remain %d, blocks: %d\r\n",__func__,remain,blocks);
    if (blocks == 0) {
	    if (!ahci_vdev_readBlock(node,offset/512,512,datBuff)) {
		    arch_mmu_switch(curSpace);
		    return -1;
	    }
	    arch_mmu_switch(curSpace);
	    memcpy(buffer,datBuff+realOffset,how);
	    prc->aspace = prcOrig;
	    arch_mmu_switch(curSpace);
	    return how;
	}
    int need = how;
    int start = offset / 512;
    if (blocks > 32) {
	    for (int i = 0; i < blocks; i+=32) {
		    // If the block count is more that 32, read in loop.
		    if (!ahci_vdev_readBlock(node,(offset+i)/512,16384,datBuff)) {
			    arch_mmu_switch(curSpace);
			    return -1;
		    }
		    arch_mmu_switch(curSpace);
		    int howCopy = i+32 > blocks ? (blocks-i)*512 : 16384;
		    memcpy(buffer,datBuff,howCopy);
		    buffer += howCopy;
		    arch_mmu_switch(prc->aspace);
	}
   } else {
	   if (!ahci_vdev_readBlock(node,start,realSize,datBuff)) {
		   arch_mmu_switch(curSpace);
		   return -1;
	   }
	   arch_mmu_switch(curSpace);
	   memcpy(buffer,datBuff,realSize);
	   buffer += realSize;
	   arch_mmu_switch(prc->aspace);
   }
    if (remain > 0) {
	    // Read remain
	    //kprintf("Remain: %d bytes\r\n",remain);
	    ahci_vdev_readBlock(node,blocks+1,512,datBuff);
	    arch_mmu_switch(curSpace);
	    memcpy(buffer,datBuff,remain);
	    arch_mmu_switch(prc->aspace);
  }
    prc->aspace = orig;
    if (reqSwitch) {
	    prc->aspace = prcOrig;
	    arch_mmu_switch(curSpace);
	}
    if (curProcIsnt) {
	    prc->aspace = prcOrig;
	}
    return how;
}

void registerDev(struct sata_info *inf) {
    dev_t *dev = kmalloc(sizeof(dev_t));
    memset(dev,0,sizeof(dev));
    // check if device is SATAPI or SATA
    if (inf->type == AHCI_DEV_SATAPI) {
		dev->name = strdup("satapi ");
    } else {
		dev->name = strdup("sata ");
    }
    int len = strlen(dev->name);
    if (inf->type == AHCI_DEV_SATAPI) {
		dev->name[len-1] = satapi_char++;
    } else {
		dev->name[len-1] = sata_char++;
    }
    dev->readBlock = ahci_vdev_readBlock;
    dev->read = ahci_vdev_read;
    dev->writeBlock = ahci_vdev_writeBlock;
    dev->write = ahci_vdev_write;
    dev->ioctl = ahci_vdev_ioctl;
    // Only SATA devices currently have power capatiable.
    dev->poweroff = ahci_vdev_poweroff;
    dev->poweron = ahci_vdev_poweron;
    dev->type = DEVFS_TYPE_POWER;
    dev->buffer_sizeMax = 512;
    dev->device = inf;
    if (inf->type == AHCI_DEV_SATA) {
	    dev->type |= DEVFS_TYPE_BLOCK;
    }
    dev->next = NULL;
    dev_add(dev);
}

void *ahci_handler(void *regs) {
    // Switch to kernel address space
    void *aspace = arch_mmu_getAspace();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    int inter_pending = ctrl_data->is & ctrl_data->pi;
    if (inter_pending == 0) {
        return regs;
    }
    int found = 0;
    // check ports
    for (int i = 0; i < 32; i++) {
        HBA_PORT *port = gl_dat->ports[i];
        if (port == NULL) continue;
        if (inter_pending & (1 << i)) {
            int is = port->is;
             port->is = is; // clear
             found = 1;
	     gl_dat->waitForIRQ[i] = false;
	     /*int ipm = (port->ssts >> 8) & 0x0F;
	     if (ipm != HBA_PORT_IPM_ACTIVE) {
		     kprintf("AHCI: Device %d disconnected\r\n",i);
		     port->ie = 0;
		     stop_cmd(port);
		}*/
        }
    }
    ctrl_data->is = inter_pending;
    if (!found) {
	    kprintf("AHCI: Strange IRQ received\r\n");
    }
    arch_mmu_switch(aspace);
    return regs;
}
int
fls(unsigned mask)
{
	if (mask == 0)
		return 0;
	int pos = 1;
	while (mask != 1) {
		mask >>= 1;
		pos++;
	}
	return pos;
}
static void reset_controller() {
    // reset HBA controller begin
    int savedCaps = ctrl_data->cap & (CAP_SMPS | CAP_SSS | CAP_SPM | CAP_EMS | CAP_SXS);
    int savedPi = ctrl_data->pi;
    ctrl_data->ghc |= GHC_AE;
    ctrl_data->ghc |= GHC_HR;
    kprintf("AHCI: %s: waiting to GHC_HR to clear\n",__func__);
    while(ctrl_data->ghc & GHC_HR);
    ctrl_data->ghc |= GHC_AE;
    ctrl_data->cap |= savedCaps;
    ctrl_data->pi = savedPi;
    if (ahciVendor == 0x8086) {
	    kprintf("AHCI: Intel Controller detected, enabling ports\r\n");
	    int portCount = max(fls(ctrl_data->pi),1+(int)((ctrl_data->cap >> CAP_NP_SHIFT) & CAP_NP_MASK));
	    if (portCount > 8) {
		    kprintf("AHCI: Too many ports detected: %d\r\n",portCount);
		    portCount = 8;
	    }
	    uint16_t pcs = PciRead16(ctrl_id,0x92);
	    kprintf("CPS: 0x%x\r\n",pcs);
	    pcs |= (0xff >> (8 - portCount));
	    PciWrite16(ctrl_id,0x92,pcs);
	}
    kprintf("AHCI: %s: controller reseted\n",__func__);
}

static void reset_port(HBA_PORT *port) {
    stop_cmd(port);
    port->serr = port->serr;
    kprintf("AHCI: %s checking if port doesn't require FORCE reset\n",__func__);
    int spin = 0;
    /*while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
	    spin++;
	}
	if (spin == 1000000)
	{*/
		kprintf("AHCI: %s: perfoming FORCE soft reset of port\n",__func__);
        	port->sctl |= 0x1;
        	// wait minimum 1 ms to init itself
		clock_setShedulerEnabled(false);
        	arch_sti(); // em, yeah, why not
        	kprintf("Waiting for Port Become Online\r\n");
		while(1) {
			uint8_t dt = port->ssts & 0x0F;
			if (dt == 3) {
				break;
			} else {
				kprintf("PORT -> %d\r\n",dt);
			}
		}
        	arch_cli();
		clock_setShedulerEnabled(true);
        	port->sctl = (port->sctl & ~HBA_PORT_DET_MASK) | SCTL_PORT_DET_NOINIT;
    /*}
    start_cmd(port);*/
}

static bool ahci_vdev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf) {
    kprintf("AHCI: %s: nope, i didn't do that for you\n",__func__);
    return false;
}
static uint64_t ahci_vdev_write(vfs_node_t *node, uint64_t blockNo,uint64_t how, void *buf) {
    kprintf("AHCI: %s: didn't supported currently\n",__func__);
    return how;
}
static void sendCommand(int cmd,HBA_PORT *port,HBA_MEM *mem,int type,uint8_t *atapiPacket,void *buffer,int buffSize) {
	// This function send non data commands to the port.
	port->is = -1;
	int cmdslot = find_cmdslot(port,mem);
	if (cmdslot == -1) {
		return;
	}
	HBA_CMD_HEADER *cmdhdr = (HBA_CMD_HEADER *)port->clb;
	cmdhdr+=cmdslot;
	cmdhdr->cfl =  sizeof(FIS_REG_H2D)/sizeof(uint32_t);
        //cmdhdr->p = 1;
        cmdhdr->prdtl = 1;
	bool copyAlign = false;
	if ((int)buffer % 1024 != 0) {
		copyAlign = true;
	}
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(cmdhdr->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
                (cmdhdr->prdtl-1)*sizeof(HBA_PRDT_ENTRY));
	if (buffer!= NULL) {
		cmdtbl->prdt_entry[0].dba = copyAlign ? (uint32_t)datBuff : (uint32_t)buffer;
		cmdtbl->prdt_entry[0].dbc = buffSize - 1;
		cmdtbl->prdt_entry[0].i = 0;
	}
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	if (type == AHCI_DEV_SATA) {
		cmdfis->command = cmd;
	} else if (type == AHCI_DEV_SATAPI) {
		// Setup ATAPI command.
		cmdfis->command = ATA_CMD_PACKET;
		// Access command table.
		memset(cmdtbl->acmd,0,15);
		if (cmd == 0x1b || cmd == 0x1c || cmd == 0x1f || cmd == 0x1a) {
			cmdtbl->acmd[0] = 0x1b;
			int oper = 0;
			if (cmd == 0x1b) {
				oper |= (1<<1);
			} else if (cmd == 0x1c) {
				oper = 0;
			} else if (cmd == 0x1f) {
				oper |= (1<<0);
			} else if (cmd == 0x1a) {
				oper |= (1<<0);
				oper |= (1<<1);
				kprintf("Tray must be closing\r\n");
			}
			cmdtbl->acmd[1] = (0<<1);
			cmdtbl->acmd[4] = oper;
		} else if (cmd == 0x25) {
			cmdtbl->acmd[0] = 0; // TEST UNIT READY
			cmdtbl->acmd[1] = (0<<1);
		} else if (cmd == 0xA8) {

		} else {
			memcpy(cmdtbl->acmd,atapiPacket,11);
		}
		cmdhdr->a = 1;
	}
	cmdfis->c = 1;
	int spin;
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
		spin++;
	}
	if (spin == 1000000) {
		kprintf("ahci_%s: port hung.\r\n",__func__);
		return;
	}
	port->ci = 1<<cmdslot;
	//kprintf("ahci_%s: waiting for command: 0x%x to complete at slot: %d\r\n",__func__,cmd,cmdslot);
	while(1) {
		// Wait until command finished.
		if ((port->ci & (1<<cmdslot)) == 0) {
                        break;
                }
                if (port->is & HBA_PxIS_TFES) {
                        kprintf("ahci_%s: command 0x%x cause device task fault\r\n",__func__,cmd);
			// Recover and unblock port after command failure.
                        port->serr = port->serr;
			stop_cmd(port);
			port->is = -1;
			port->ie = 0;
			start_cmd(port);
			port->ie = 0xffffffff;
                        return;
                }
	}
	if (copyAlign) {
		memcpy(buffer,datBuff,buffSize);
	}
}
uint64_t ahci_vdev_ioctl(vfs_node_t *node,int request,va_list args) {
	process_t *prc = thread_getThread(thread_getCurrent());
	struct sata_info *info = (struct sata_info *)node->device;
	void *aspace = arch_mmu_getAspace();
	va_list args_orig = args; // Yes.
	arch_mmu_switch(arch_mmu_getKernelSpace());
	prc->aspace = arch_mmu_getKernelSpace();
	arch_sti();
	//kprintf("AHCI: Received ioctl: 0x%x\r\n",request);
	switch(request) {
		case 0x10:
			sendCommand(0xE0,info->port,info->abar,info->type,NULL,NULL,0);
		break;
		case 0x20:
			sendCommand(0xE1,info->port,info->abar,info->type,NULL,NULL,0);
			break;
		case 0x30: {
				// Allocate 500M of space. Nano editor spacing.
				// try to trigger parttab
				kprintf("Reading 500 MB of hard drive starting from sector 0\r\n");
				int size = 500 * 1024 * 1024;
				char *buff = kmalloc(size);
				int blocks = size / 512;
				int loop = blocks / 32;
				int start = 0;
				for (int i = 0; i < loop; i++) {
					ahci_vdev_readBlock(node,start/512,16384,buff+start);
					start+=16384;
				}
				kprintf("completed task\r\n");
				for (int i = 0; i < 512; i++) {kprintf("0x%x ",buff[i]);}
				kprintf("\r\n");
				kfree(buff);
				// Are we done? At this point no requirements to switch up to kernel space.
				kprintf("IRQ Line? 0x%x\r\n",PciRead8(ctrl_id,0x3c));
			}
			break;
		case 0x31: {
			// 0x31: execute non-data ATA/ATAPI command.
			arch_mmu_switch(aspace);
			int cmd = va_arg(args,int);
			kprintf("AHCI: Begin execution of ATA/ATAPI command: 0x%x %d\r\n",cmd);
			arch_mmu_switch(arch_mmu_getKernelSpace());
			sendCommand(cmd,info->port,info->abar,info->type,NULL,NULL,0);
		} break;
		case 0x32: {
				   // trigger the mbr module
				   partTab_trigger(node);
				} break;
	}
end:
	prc->aspace = aspace;
	arch_mmu_switch(aspace);
	return 0;
}
static void ahci_mapPort(HBA_PORT *port) {
	int begin = (int)port;
        for (int i = 0; i < 2; i++) {
               int mem = begin + (i * 4096);
                arch_mmu_mapPage(NULL,mem,mem,7);
        }
}
static bool ahci_vdev_poweroff(vfs_node_t *node) {
	// Currently only kernel code has access to power managment functions.
	// But for any case switch to kernel address space.
	process_t *prc = thread_getThread(thread_getCurrent());
	void *aspace = prc->aspace;
	void *krnSpace = arch_mmu_getKernelSpace();
	arch_mmu_switch(krnSpace);
	// Now we can call sendCommand
	struct sata_info *info = node->device;
	if (info->type == AHCI_DEV_SATAPI) {
		sendCommand(0x1c,info->port,info->abar,info->type,NULL,NULL,0);
	} else {
		sendCommand(0xE0,info->port,info->abar,info->type,NULL,NULL,0);
	}
	//kprintf("executed,switch....");
	arch_mmu_switch(aspace);
	//kprintf("ok");
	return true;
}
static bool ahci_vdev_poweron(vfs_node_t *node) {
	// like ahci_vdev_poweroff.
	process_t *prc = thread_getThread(thread_getCurrent());
	void *aspace = prc->aspace;
	void *krnSpace = arch_mmu_getKernelSpace();
	prc->aspace = krnSpace;
	arch_sti();
	arch_mmu_switch(krnSpace);
	struct sata_info *info = node->device;
	sendCommand(0xE1,info->port,info->abar,info->type,NULL,NULL,0);
	prc->aspace = aspace;
	arch_mmu_switch(aspace);
	return true;
}

