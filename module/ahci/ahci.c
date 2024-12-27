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
#define HBA_PxCMD_POD   (1 << 2)
#define HBA_PxCMD_ICC_MASK (0xf<<28)
#define HBA_PxCMD_ACTIVE (1<<28)
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#ifdef AS_MODULE
__attribute__((section(".modname"))) char name[] = "ahci";
#endif
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
        uint64_t sectors;
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
#define SCTL_PORT_DET_NOINIT	0x00000000	// no initalization request
#define SCTL_PORT_DET_INIT		0x00000001	// perform interface initalization
#define SCTL_PORT_DET_DISABLE	0x00000004	// disable phy
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
uintptr_t ahci_handler(uintptr_t regs);
static char *datBuff[16384] __attribute__((aligned(1024)));
static uintptr_t datBuffPhys = 0;
bool identifyDisk(HBA_PORT *port,HBA_MEM *abar,int type,struct sata_info *info);
static int AHCI_BASE = 0;
static struct glob *gl_dat;
static void reset_controller();
static void reset_port(HBA_PORT *port);
// Sends non-data command to target port using device type
//	     Send the non-data command to device				if device isn't ATAPI this parameter can
//	     									be NULL.
//	     										   Buffer is used for non read
//	     										   commands(use read_sata instead)
static void sendCommand(int cmd,struct sata_info *info,uint8_t *atapiPacket,void *buffer,int buffSize);
static int ahciVendor;
uint64_t ahci_vdev_ioctl(vfs_node_t *node,int request,va_list arg);
static char satapi_char = '0';
static char sata_char = '0';
static void ahci_mapPort(HBA_PORT *port);
static bool ahci_vdev_poweroff(vfs_node_t *node);
static bool ahci_vdev_poweron(vfs_node_t *node);
static bool setupAtapi(HBA_PORT *port,HBA_MEM *abar);
static bool sendATAPICommand(HBA_PORT *port,uint8_t *cmd,int size,void *buffer);
void stop_cmd(HBA_PORT *port);
bool start_cmd(HBA_PORT *port);
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
        uintptr_t addr = PciRead32(id,PCI_CONFIG_BAR5);
	PciWrite16(id,PCI_CONFIG_COMMAND,PciRead16(id,PCI_CONFIG_COMMAND) | 0x0002 | 0x0004);
        uintptr_t map_addr = arch_mmu_get_io_addr(sizeof(HBA_MEM)+4096);
        // Map the space
        int pages = 2;
        for (int page = 0; page < pages; page++) {
            int align = page*4096;
            arch_mmu_mapPage(NULL,map_addr+align,addr+align,3);
        }
        ctrl_data = (HBA_MEM *)map_addr;
        kprintf("ahci: addr => 0%x\n",ctrl_data);
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
	if (ipm != HBA_PORT_IPM_ACTIVE) {
		// Try to perform AHCI COMRESET.
		reset_port(port);
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
        stop_cmd(port);
        port->serr =1;//For each implemented port, clear the PxSERR register, by writing 1 to each mplemented location
        port->is=0;//
        port->ie = 0;
        info->clb = (void *)AHCI_BASE + (portno<<10);
        port->clb = (uintptr_t)arch_mmu_getPhysical(info->clb);
        port->clbu = 0;
        memset((void*)(info->clb), 0, 1024);
        // FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	info->fb = (void *)AHCI_BASE + (32<<10) + (portno<<8);
        port->fb = (uintptr_t)arch_mmu_getPhysical(info->fb);
	port->fbu = 0;
	memset((void*)(info->fb), 0, 256);
	// Command table offset: 40K + 8K*portno
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(port->clb);
	for (int i=0; i<32; i++)
	{
		cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
					// 256 bytes per command table, 64+16+48+16*8
		// Command table offset: 40K + 8K*portno + cmdheader_index*256
		info->ctba[i] = (void *)AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
                cmdheader[i].ctba = (uintptr_t)arch_mmu_getPhysical(info->ctba[i]);
		cmdheader[i].ctbau = 0;
		memset(info->ctba[i], 0, 256);
	}
	port->cmd |= (1<<28);
    	start_cmd(port);
        port->is = 0;
        port->ie = 0xffffffff;
}
#ifdef AS_MODULE
static void module_main() {
#else
void ahci_init() {
#endif
    kprintf("AHCI TEST DRIVER(V2.01.0001)!!!\n");
    // Prepare datBuff?
    datBuffPhys = (uintptr_t)arch_mmu_getPhysical(datBuff);
    memset(datBuff,0,16384);
    kprintf("datBuffPhys = 0x%x\n",datBuffPhys);
    kprintf("checking the alignment: 1024: %d\n",((uintptr_t)datBuffPhys % 1024));
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
    int pages = 4096;
    /*AHCI_BASE = arch_mmu_get_io_addr(pages * 4096);
    if (AHCI_BASE == -1) {
            kprintf("looks like your system doesn't have enought memory for the AHCI driver. Cannot continue\n");
            return;
        }*/
    uintptr_t firstPage = alloc_getPage();
    AHCI_BASE = firstPage;
    arch_mmu_mapPage(NULL,AHCI_BASE,firstPage,3);
    //alloc_reserve(AHCI_BASE,AHCI_BASE+(4090*4096));
    for (int i = 1; i < pages; i++) {
	    int page = alloc_getPage();
            int addr = AHCI_BASE + (i * 4096);
            arch_mmu_mapPage(NULL,addr,page,3);
	}
    // arch_mmu_mapPage can allocate new PT, so we need to make 2 different loops.
    int pi = ctrl_data->pi;
    int i =0;
    struct sata_info *sata = NULL;
    gl_dat = (struct glob *)kmalloc(sizeof(struct glob));
    memset(gl_dat,0,sizeof(struct glob));
    arch_sti();
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
			if (!identifyDisk(port,ctrl_data,AHCI_DEV_SATA,sata)) {
                                kprintf("ahci: device identification failure!\n");
			}
                        ata_identify_t *ident = (ata_identify_t *)datBuff;
                        sata->sectors = ident->sectors_48;
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
			 port->cmd &= ~(1<<25);
			 sata->port = port;
			 //sendCommand(0x1a,port,ctrl_data,AHCI_DEV_SATAPI,NULL,NULL,0);
			 identifyDisk(port,ctrl_data,AHCI_DEV_SATAPI,sata);
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
        arch_sti();
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
bool identifyDisk(HBA_PORT *port,HBA_MEM *abar,int type,struct sata_info *info) {
	// clean any interrupts before seting up new command.
	port->is = (uint32_t)-1;
	int spin; // port hung detection.
	int cmdslot = find_cmdslot(port,abar);
	if (cmdslot == -1) {
		// very strange.
		return false;
	}
        uint32_t phs = (uint32_t)(uintptr_t)arch_mmu_getPhysical(datBuff);
        kprintf("identifyDisk -> 0x%x(phs => 0x%x)\n",datBuffPhys,phs);
	HBA_CMD_HEADER *cmd = (HBA_CMD_HEADER *)info->clb;
	cmd+=cmdslot;
	cmd->cfl =  sizeof(FIS_REG_H2D)/sizeof(uint32_t);
        cmd->w = 0;
	//if (type == AHCI_DEV_SATAPI) cmd->a = 1;
	cmd->p = 0;
	cmd->prdtl = 1;
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(info->ctba[cmdslot]);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
 		(cmd->prdtl-1)*sizeof(HBA_PRDT_ENTRY));
	cmdtbl->prdt_entry[0].dba = phs;
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

	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)pdata->clb;
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
	cmdheader->w = write;		// Read from device
    	cmdheader->c = 0;               // Read from device
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;	// PRDT entries count

	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)(pdata->ctba[slot]);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
 		(cmdheader->prdtl-1)*sizeof(HBA_PRDT_ENTRY));

	// 8K bytes (16 sectors) per PRDT
    	int i = 0;
	for (i=0; i<cmdheader->prdtl-1; i++)
	{
		cmdtbl->prdt_entry[i].dba = (uint32_t) (uintptr_t)buf;
		cmdtbl->prdt_entry[i].dbc = 8*1024-1;	// 8K bytes (this value should always be set to 1 less than the actual value)
		cmdtbl->prdt_entry[i].i = 1;
		buf += 4*1024;	// 4K words
		count -= 16;	// 16 sectors
	}
	// Last entry
	cmdtbl->prdt_entry[i].dba = (uint32_t)(uintptr_t)buf;
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
	//while(gl_dat->waitForIRQ[pdata->portID]);
	// Okay, we handled IRQ.
	while (1)
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
	}
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
        int sa = 16384 / block_size;
	// Here by some reason the interrupts begin disabled.
	// Make sure we working in kernel space.
        bool ret = false;
read:
	for (int i = 0; i < 1; i++) {
    		int blocks = how / block_size;
    		if (blocks == 0) {
        		blocks = 1;
    		}
		if (how > 16384) {
			kprintf("ahci: debug, read %d bytes\n",how);
                        while(how <= 0) {
                                if (!rw_sata(inf,blockNo,sa,(void *)datBuffPhys,true,false)) {
                                        goto clean;
                                }
                                memcpy(buf,datBuff,16384);
                                buf+=16384;
                                how-=16384;
                        }
                } else {
                        if (!rw_sata(inf,blockNo,blocks,(void *)datBuffPhys,true,false)) {
                                goto clean;
                        }
                        //kprintf("Copying %d bytes\n",how);
                        // TEST!
                        asm volatile("cld; rep movsb" : "=c"((int){0}) : "D"(buf),"S"(datBuff),"c"(how) : "flags","memory");
                }
	}
	ret = true;
clean:
	return ret;
}

static uint64_t ahci_vdev_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buffer) {
    struct sata_info *inf = (struct sata_info *)node->device;
    if (inf->type == AHCI_DEV_SATAPI) return 0;
    int remain = how % 512;
    int realSize = how - remain;
    int blocks = realSize / 512;
    int realOffset = offset % 512;
read:
    //kprintf("%s: remain %d, blocks: %d\r\n",__func__,remain,blocks);
    if (blocks == 0) {
	    if (!ahci_vdev_readBlock(node,offset/512,512,datBuff)) {
		    return -1;
	    }
	    memcpy(buffer,datBuff+realOffset,how);
	    return how;
	}
    int need = how;
    int start = offset / 512;
    if (blocks > 32) {
	    for (int i = 0; i < blocks; i+=32) {
		    // If the block count is more that 32, read in loop.
		    if (!ahci_vdev_readBlock(node,(offset+i)/512,16384,datBuff)) {
			    return -1;
		    }
		    int howCopy = i+32 > blocks ? (blocks-i)*512 : 16384;
		    memcpy(buffer,datBuff,howCopy);
		    buffer += howCopy;
	}
   } else {
	   if (!ahci_vdev_readBlock(node,start,realSize,datBuff)) {
		   return -1;
	   }
	   memcpy(buffer,datBuff,realSize);
	   buffer += realSize;
   }
    if (remain > 0) {
	    // Read remain
	    //kprintf("Remain: %d bytes\r\n",remain);
	    ahci_vdev_readBlock(node,blocks+1,512,datBuff);
	    memcpy(buffer,datBuff,remain);
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
    // Now change the dev size!
    if (inf->type != AHCI_DEV_SATAPI && inf->sectors > 0) {
            uint64_t size = inf->sectors * 512;
            vfs_node_t *n = (vfs_node_t *)dev->devNode;
            n->size = size;
    }
}

uintptr_t ahci_handler(uintptr_t stack) {
    // Switch to kernel address space
    int inter_pending = ctrl_data->is & ctrl_data->pi;
    if (inter_pending == 0) {
	return stack;
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
	     if (gl_dat->waitForIRQ[i] < 255) {
		     // If waitForIRQ[i] == 255 then that means that the device is waiting for hotplug not for operation end.
		     gl_dat->waitForIRQ[i] = false;
	     }
	     /*
	      * From AHCI specifications(section 7.1.1)
	      * Software should then check PxCMD.CPS to determine whether a device is present.
	     */
	     if ((port->is & (1<<6)) == (1<<6)) {
		     kprintf("AHCI: Physical port #%d %s\n",i,(gl_dat->waitForIRQ[i] < 255 ? "disconnected" : "connected"));
		     if (gl_dat->waitForIRQ[i] < 255) {
			     kprintf("AHCI: Stopping command engine\n");
			     stop_cmd(port);
		    } else {
			    gl_dat->waitForIRQ[i] = 0;
			}
		     // AHCI specifications section 7.3.1.1
		     port->sctl |= (SCTL_PORT_IPM_NOPART | SCTL_PORT_IPM_NOSLUM);
		     port->cmd |= ~(1<<26); // Disable aggresive link power managment(if available)
		     port->serr = port->serr;
		     port->ie |= (1<<22); // enable port connection interrupt only.
		     port->is = port->is;
		     kprintf("AHCI: Physical Port #%d %s\n",i,(gl_dat->waitForIRQ[i] < 255 ? "disabled" : "enabled"));
		     // Notify ourself that the port is disabled.
		     if (gl_dat->waitForIRQ[i] == 0) {gl_dat->waitForIRQ[i] = 255;}
		}
        }
    }
irq_end:
    ctrl_data->is = inter_pending;
    if (!found) {
	    kprintf("AHCI: Strange IRQ received\r\n");
    }
    return stack;
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
	kprintf("AHCI: Reseting port\n");
	stop_cmd(port);
	port->sctl |= (SCTL_PORT_IPM_NOPART | SCTL_PORT_IPM_NOSLUM); // We doesn't allow the port to switch it's power state.
	port->is = port->is;
	 // Power on device!
        port->cmd |= HBA_PxCMD_POD;
        // Spin up.
        port->cmd |= HBA_PxCMD_SUD;
        // Activate link.
        port->cmd |= (port->cmd & ~HBA_PxCMD_ICC_MASK) | HBA_PxCMD_ACTIVE;
        port->serr = port->serr;
	port->sctl = (SSTS_PORT_IPM_ACTIVE | SSTS_PORT_IPM_PARTIAL | SCTL_PORT_DET_INIT);
        arch_sti();
        kwait(10);
        arch_cli();
	 // We must wait at least 1 MS!
        port->sctl = (port->sctl & ~HBA_PORT_DET_MASK) | SCTL_PORT_DET_NOINIT;
	kprintf("port ssts->det: %d\n",port->ssts,port->ssts & 0x0F);
        arch_sti();
        int i = 0;
        for (i = 0; i < 50; i++) {
                if ((port->ssts & 0x0F) != 3) {
                        //kprintf("waiting for the port\n");
                        kwait(100);
                } else {
                        break;
                }
        }
	arch_cli();
	if (i == 50) {
		kprintf("AHCI: Port reset failure\n");
		return;
	} else {
		kprintf("AHCI: Port COMRESET perfomed successfully\n");
	}
	port->serr = port->serr; // clear error.
}

static bool ahci_vdev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf) {
    struct sata_info *info = (struct sata_info *)node->device;
    if (info->type == AHCI_DEV_SATAPI) return false;
    int blksize = 16384;
    int blocks = how/blksize;
    int ret = false;
    if (blocks == 0) {
            blocks = (how / 512);
            if (blocks == 0) blocks = 1;
	    memcpy(datBuff,buf,how);
	    if (!rw_sata(info,blockNo,blocks,(void *)datBuffPhys,true,true)) {
		    return false;
	    }
    } else {
	    for (int i = 0; i < blocks; i++) {
		    memcpy(datBuff,buf,16384);
		    if (!rw_sata(info,blockNo,32,(void *)datBuffPhys,true,true)) {
			    return false;
		    }
	    }
    }
    ret = true;
    return ret;
}
static uint64_t ahci_vdev_write(vfs_node_t *node, uint64_t blockNo,uint64_t how, void *buf) {
    kprintf("AHCI: %s: didn't supported currently\n",__func__);
    return how;
}
static void sendCommand(int cmd,struct sata_info *info,uint8_t *atapiPacket,void *buffer,int buffSize) {
	// This function send non data commands to the port.
        HBA_PORT *port = info->port;
        HBA_MEM *mem = info->abar;
        int type = info->type;
	port->is = -1;
	int cmdslot = find_cmdslot(port,mem);
	if (cmdslot == -1) {
		return;
	}
	// Map it.
	extern uint64_t krnVirtualOffset;
	HBA_CMD_HEADER *cmdhdr = (HBA_CMD_HEADER *)info->clb;
	cmdhdr+=cmdslot;
	cmdhdr->cfl =  sizeof(FIS_REG_H2D)/sizeof(uint32_t);
        //cmdhdr->p = 1;
        cmdhdr->prdtl = 1;
	bool copyAlign = false;
	if ((uintptr_t)buffer % 1024 != 0) {
		copyAlign = true;
	}
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL *)(info->ctba[cmdslot]);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) +
                (cmdhdr->prdtl-1)*sizeof(HBA_PRDT_ENTRY));
	if (buffer!= NULL) {
		cmdtbl->prdt_entry[0].dba = copyAlign ? (uint32_t)(uintptr_t)datBuffPhys : (uint32_t)(uintptr_t)buffer;
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
	struct sata_info *info = (struct sata_info *)node->device;
	va_list args_orig; // Yes.
	va_copy(args_orig,args);
	//kprintf("AHCI: Received ioctl: 0x%x\r\n",request);
	return 0;
}
static void ahci_mapPort(HBA_PORT *port) {
	vaddr_t begin = (vaddr_t)port;
        for (int i = 0; i < 2; i++) {
               vaddr_t mem = begin + (i * 4096);
                arch_mmu_mapPage(NULL,mem,mem,3);
        }
}
static bool ahci_vdev_poweroff(vfs_node_t *node) {
	// Now we can call sendCommand
	struct sata_info *info = node->device;
	//ahci_mapPort(info->port);
	if (info->type == AHCI_DEV_SATAPI) {
		sendCommand(0x1c,info,NULL,NULL,0);
	} else {
		sendCommand(0xE0,info,NULL,NULL,0);
	}
	return true;
}
static bool ahci_vdev_poweron(vfs_node_t *node) {
	// like ahci_vdev_poweroff.
	arch_sti();
	struct sata_info *info = node->device;
poweroff:
	sendCommand(0xE1,info,NULL,NULL,0);
	return true;
}

