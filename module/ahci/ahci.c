#include <output.h>
#include <debug.h>
#include <pci/driver.h> // PCI
#include <pci/registry.h>
#include <mm/alloc.h>
#include <arch/mmu.h>
#include <arch.h>
#include <dev.h>
#include <mm/alloc.h>

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
    int devID;
};

struct glob {
    HBA_MEM *regs;
    int pi;
    HBA_MEM *ports[32];
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


static HBA_MEM *ctrl_data;
static PciBar *bar;
static int controllerID;

static int find_cmdslot(HBA_PORT *port,HBA_MEM *abar);
static uint8_t read_sata(struct sata_info *pdata, uint32_t startl, uint32_t starth, uint32_t count,char *buf);
static void ahci_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf);
static void ahci_vdev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf);
static int ahci_vdev_write(vfs_node_t *node,int blockNo,int how, void *buf);
void registerDev(struct sata_info *inf);
void* align_addr(void* address,int al);
void *ahci_handler(void *regs);
char datBuff[512] __attribute__((aligned(1024)));

extern int AHCI_BASE;
static struct glob *gl_dat;
static void reset_controller();
static void reset_port(HBA_PORT *port);

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
            kprintf("found\r\n");
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
    uint8_t interLine = PciRead8(id,0x3c);
    kprintf("INTER LINE: 0x%x\n",interLine);
    interrupt_add(interLine,ahci_handler);
	/*kprintf("AHCI controller PCI dump: \n");
	PciBar *ba = kmalloc(sizeof(PciBar));
	memset(ba,0,sizeof(PciBar));
	for (int i = 0; i < 5; i++) {
		PciGetBar(ba,id,i);
		kprintf("BAR%d dump: \n",i);
		kprintf("Address 0x%x\n",ba->u.address);
		kprintf("Size: 0x%x\n",ba->size);
		kprintf("Flags: 0x%x\n",ba->flags);
	}*/
      kprintf("Controller vendor ID: 0x%x\n",PciRead16(id,PCI_CONFIG_VENDOR_ID));
      kprintf("WW: 0x%x\n",PciRead16(id,0x92));
      controllerID = id;
    }
}

static int check_type(HBA_PORT *port) {
	uint16_t ssts = port->ssts;
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;
	if (det != HBA_PORT_DET_PRESENT) {
		return AHCI_DEV_NULL;
	}
	if (ipm != HBA_PORT_IPM_ACTIVE) {
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

void start_cmd(HBA_PORT *port)
{
    // Set FRE (bit4) and ST (bit0)
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR)
        ;
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
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
    kprintf("AHCI base reported by kernel: 0x%x\n",AHCI_BASE);
    // Reset controller and it's ports
    kprintf("Controller reseted, tfd: 0x%x\n",port->tfd);
    kprintf("stop command processor, tfd: 0x%x\n",port->tfd);
    kprintf("Reseting port\n");
    port->cmd |= HBA_PxCMD_CR;
    while(port->cmd & HBA_PxCMD_CR);
    port->serr =1;//For each implemented port, clear the PxSERR register, by writing 1 to each mplemented location
    port->is=0;//
    port->ie = 0;
    kprintf("WW: 0x%x\n",PciRead16(info->devID,0x92));
    port->clb = AHCI_BASE + (portno<<10);
	port->clbu = 0;
	memset((void*)(port->clb), 0, 1024);

    info->clb = port->clb;
	// FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	port->fb = AHCI_BASE + (32<<10) + (portno<<8);
	port->fbu = 0;
	memset((void*)(port->fb), 0, 256);
    info->fb = port->fb;
    kprintf("cmdheader pre setup, tfd: 0x%x\n",port->tfd);
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
    kprintf("Waiting for the bit15 to be cleared\n");
    while (port->cmd & HBA_PxCMD_CR);
    reset_port(port);
    kprintf("Cleared by controller, continue, tfd: 0x%x\n",port->tfd);
    //start_cmd(port);
    port->is = 0;   
    port->ie = 0xffffffff;
    kprintf("AHCI: TFD(after rebase done): 0x%x\n",port->tfd);
}

void ahci_main() {
    arch_cli();
    memset(datBuff,0,512);file:///home/servadmin/HelinKern/kernel.bin
    kprintf("BUFF: 0x%x\n",datBuff);
    kprintf("Finding AHCI controller in PCI base...");
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
        kprintf("not found\r\n");
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
        // reset controller
        reset_controller();
        if (!(ctrl_data->ghc & 0x80000000)) {
            kprintf("Switching AHCI to AHCI mode\n");
        }
        interrupt_add(0x2b,ahci_handler);
    while(i < 32) {
	if (pi & 1) {
        HBA_PORT* port = (HBA_PORT*)&ctrl_data->ports[i];
		int type = check_type(port);
		if (type == AHCI_DEV_SATA) {
			kprintf("Found hard drive on port %d\r\n",i);
			kprintf("Port CLB: 0x%x, CLBU: 0x%x\r\n",port->clb,port->clbu);
			sata = kmalloc(sizeof(struct sata_info));
			memset(sata,0,sizeof(struct sata_info));
			kprintf("Rebasing port\n");
			sata->abar = ctrl_data;
            sata->devID = controllerID;
			rebase_port(port,i,sata);
            sata->port = port;
            // ahaha, map the memory for port
            int begin = (int)port;
            for (int i = 0; i < 2; i++) {
                int mem = begin + (i * 4096);
                arch_mmu_mapPage(NULL,mem,mem,7);
            }
		} else if (type == AHCI_DEV_SATAPI) {
           		 kprintf("Found CDROM drive on port %d\r\n",i);
        	} else {
            		pi >>= 1;
			i++;
			continue;
        	}
	if (type == AHCI_DEV_SATA) {
		/*kprintf("Reading first sector(test)\n");
		char *buff = kmalloc(512);
        buff[0] = 's';
        buff[1] = 'a';
        //void *sec_b = align_address(buff,2);
		int ret = read_sata(sata,0,0,1,buff);
        for (int i = 0; i < 512; i++) {
            kprintf("0x%x ",buff[i]);
        }
        kprintf("\n");
		kfree(buff);
		kprintf("Is read successfull: %d\n",ret);*/
        gl_dat->pi = pi;
        gl_dat->ports[i] = port;
        registerDev(sata);
	}
	}
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

static uint8_t read_sata(struct sata_info *pdata, uint32_t startl, uint32_t starth, uint32_t count,char *bufa) {
	HBA_PORT *port = pdata->port;
    // The address must be aligned up to 1KiB!
    int realAddr = (int)bufa;
    char *buf = NULL;
    bool useAligned = false;
    if ((realAddr % 1024) != 0) {
        //kprintf("AHCI: Using prebuild aligned buffer!\n");
        buf = datBuff;
        useAligned = true;
    } else {
        buf = bufa;
    }
    port->is = (uint32_t) -1;		// Clear pending interrupt bits
    //kprintf("AHCI: [DEBUG]: Pre FIS: TFD: 0x%x\n",port->tfd);
	int spin = 0; // Spin lock timeout counter
	int slot = find_cmdslot(port,pdata->abar);
	if (slot == -1)
		return false;
 
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)port->clb;
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
	cmdheader->w = 0;		// Read from device
    cmdheader->c = 1;               // Read from device
        cmdheader->p = 1;               // Read from devic
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
		cmdtbl->prdt_entry[i].i = 0;
		buf += 4*1024;	// 4K words
		count -= 16;	// 16 sectors
	}
	// Last entry
	cmdtbl->prdt_entry[i].dba = (uint32_t) buf;
	cmdtbl->prdt_entry[i].dbc = (count<<9)-1;	// 512 bytes per sector
	cmdtbl->prdt_entry[i].i = 0;
 
	// Setup command
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);
 
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;	// Command
	cmdfis->command = ATA_CMD_READ_DMA_EX;
 
	cmdfis->lba0 = (uint8_t)startl;
	cmdfis->lba1 = (uint8_t)(startl>>8);
	cmdfis->lba2 = (uint8_t)(startl>>16);
	cmdfis->device = 1<<6;	// LBA mode
 
	cmdfis->lba3 = (uint8_t)(startl>>24);
	cmdfis->lba4 = (uint8_t)starth;
	cmdfis->lba5 = (uint8_t)(starth>>8);
 
	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;
 
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
 
	port->ci = 1<<slot;	// Issue command
 
	// Wait for completion
	while (1)
	{
		// In some longer duration reads, it may be helpful to spin on the DPS bit 
		// in the PxIS port field as well (1 << 5)
		if ((port->ci & (1<<slot)) == 0) 
			break;
		if (port->is & HBA_PxIS_TFES)	// Task file error
		{
			kprintf("Read disk error\n");
			return false;
		}
	}
 
	// Check again
	if (port->is & HBA_PxIS_TFES)
	{
		kprintf("Read disk error\n");
		return false;
	}
    if (useAligned) {
        memcpy(bufa,datBuff,512);
    }
   // kprintf("Port isn't hung, tfd: 0x%x, serr: 0x%x, ssst: 0x%x, cmd: 0x%x\n",port->tfd,port->serr,port->ssts,port->cmd);
	return true;
}

void blockNoToLBA(uint32_t blockNo, uint32_t blockSize, uint32_t* startl, uint32_t* starth) {
    // Calculate the LBA (Logical Block Address) based on block number and block size
    uint32_t lba = blockNo * (blockSize / 512);

    // Break down LBA into startl and starth
    *startl = lba & 0xFFFFFFFF;
    *starth = (lba >> 32) & 0xFFFFFFFF;
}

static void ahci_vdev_readBlock(vfs_node_t *node,int blockNo,int how, void *buf) {
    void *old = arch_mmu_getAspace();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    // Convert blockNo to the startl and starth
    int blocks = how / 512;
    if (blocks == 0) {
        blocks = 1;
    }
    int startl,starth;
    struct sata_info *inf = (struct sata_info *)node->device;
    blockNoToLBA(blockNo,512,&startl,&starth);
    // get physical address of buffer
    char *bufReal = (char *)buf;
    read_sata(inf,startl,0,blocks,bufReal);
    arch_mmu_switch(old);
}

static int ahci_vdev_read(vfs_node_t *node,uint64_t offset,uint64_t how,void *buffer) {
    int start_block = (offset / 512);
    int end_block = (offset + how -1 ) / 512;
	int b_offset = 0;
	if (offset % 512 || how < 512) {
		unsigned int pr_size = (512 - (offset % 512));
		if (pr_size > how) pr_size = how;
		char *tmp = kmalloc(512);
		ahci_vdev_readBlock(node,start_block,512,tmp);
		memcpy(buffer, (void *)((uintptr_t)tmp + ((uintptr_t)offset % 512)), pr_size);
		kfree(tmp);
		b_offset += pr_size;
		start_block++;
	}
	if ((offset + how)  % 512 && start_block <= end_block) {
		int prr_size = (offset + how) % 512;
		char *tmp = kmalloc(512);
        ahci_vdev_readBlock(node,end_block,512,tmp);
        memcpy((void *)((uintptr_t)buffer + how - prr_size), tmp, prr_size);
		kfree(tmp);
		end_block--;
	}
	while(start_block <= end_block) {
		ahci_vdev_readBlock(node,start_block,512,(void *)buffer + b_offset);
		b_offset+=512;
		start_block++;
	}
	return how;
}

void registerDev(struct sata_info *inf) {
    dev_t *dev = kmalloc(sizeof(dev_t));
    memset(dev,0,sizeof(dev));
    dev->name = "sata0";
    dev->readBlock = ahci_vdev_readBlock;
    dev->read = ahci_vdev_read;
    dev->writeBlock = ahci_vdev_writeBlock;
    dev->write = ahci_vdev_write;
    dev->buffer_sizeMax = 512;
    dev->device = inf;
    dev->type = DEVFS_TYPE_BLOCK;
    dev->next = NULL;
    dev_add(dev);
}

void *ahci_handler(void *regs) {
    int inter_pending = ctrl_data->is & ctrl_data->pi;
    if (inter_pending == 0) {
        return regs;
    }
    // check ports
    for (int i = 0; i < 32; i++) {
        HBA_PORT *port = gl_dat->ports[i];
        if (port == NULL) continue;
        if (inter_pending & (1 << i)) {
            int is = port->is;
             port->is = is; // clear
        }
    }
    ctrl_data->is = inter_pending;
    return regs;
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
    kprintf("AHCI: %s: controller reseted\n",__func__);
}

static void reset_port(HBA_PORT *port) {
    stop_cmd(port);
    port->serr = port->serr;
    kprintf("AHCI: %s checking if port doesn't require FORCE reset\n",__func__);
    int spin = 0;
    while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	if (spin == 1000000)
	{
		kprintf("AHCI: %s: perfoming FORCE soft reset of port\n",__func__);
        port->sctl = (SSTS_PORT_IPM_ACTIVE | SSTS_PORT_IPM_PARTIAL
			| SCTL_PORT_DET_INIT);
        // wait minimum 1 ms to init itself
        arch_sti(); // em, yeah, why not
        kwait(1100);
        arch_cli();
        port->sctl = (port->sctl & ~HBA_PORT_DET_MASK) | SCTL_PORT_DET_NOINIT;
	}
    start_cmd(port);
}

static void ahci_vdev_writeBlock(vfs_node_t *node,int blockNo,int how,void *buf) {
    kprintf("AHCI: %s: nope, i didn't do that for you\n",__func__);
}
static int ahci_vdev_write(vfs_node_t *node,int blockNo,int how, void *buf) {
    kprintf("AHCI: %s: didn't supported currently\n",__func__);
    return how;
}
