#include <output.h>
#include <debug.h>
#include <pci/driver.h> // PCI
#include <pci/registry.h>
#include <mm/alloc.h>

__attribute__((section(".modname"))) char *name = "ahci";

static int ctrl_id = 0;


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

static HBA_MEM *ctrl_data;
static PciBar *bar;

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
	    bar = kmalloc(sizeof(PciBar));
	    memset(bar,0,sizeof(PciBar));
        ctrl_id = id;
	    ctrl_data = (HBA_MEM *)PciRead32(id,(0x24|0x0));
        kprintf("ctrl_data: 0x%x, version: %d\r\n",ctrl_data,ctrl_data->vs);
	    kprintf("PCI device info: Read address(BAR5): [0x%x], Read Address:c [0x%x]\r\n",PciRead32(id,(0x24 | 0x0)),PciRead32(id,(0x3c|0x0))& 0x000000000000ff00);
    }
}

static int check_type(HBA_PORT *port) {
	uint16_t ssts = port->ssts;
	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;
	kprintf("ipm %d, det %d, sig: %d\r\n",ipm,det,port->sig);
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


static void module_main() {
    DEBUG_N("Finding controller in PCI base...");
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
    while(i < 32) {
	if (pi & 1) {
		int type = check_type((HBA_PORT *)&ctrl_data->ports[i]);
		if (type == AHCI_DEV_SATA) {
			kprintf("Found hard drive on port %d\r\n",i);
		} else {
			kprintf("Unknown type: %d\r\n",type);
		}
	}
	pi >>= 1;
	i++;
    }
    kfree(bar);
}
