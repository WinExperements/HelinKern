// ------------------------------------------------------------------------------------------------
// pci/pci.c
// ------------------------------------------------------------------------------------------------

#include "pci/pci.h"
#include <output.h>
#include "pci/registry.h"
#include "pci/driver.h"
#include <arch/x86/idt.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <lib/queue.h>
//char modname[] __attribute__((section(".modname"))) = "pci";

// We store each PCI device info to make sure that the search process is done.
static queue_t *PciQueue;
// ------------------------------------------------------------------------------------------------
static void PciVisit(unsigned int bus, unsigned int dev, unsigned int func)
{
    unsigned int id = PCI_MAKE_ID(bus, dev, func);

    PciDeviceInfo *info = NULL;
    uint16_t vendorId= PciRead16(id, PCI_CONFIG_VENDOR_ID);
    if (vendorId == 0xffff)
    {
        return;
    }
    info = kmalloc(sizeof(PciDeviceInfo));
    info->vendorId = vendorId;
    info->baseID = id;
    info->deviceId = PciRead16(id, PCI_CONFIG_DEVICE_ID);
    info->progIntf = PciRead8(id, PCI_CONFIG_PROG_INTF);
    info->subclass = PciRead8(id, PCI_CONFIG_SUBCLASS);
    info->classCode = PciRead8(id, PCI_CONFIG_CLASS_CODE);
    enqueue(PciQueue,info);
    kprintf("ID: %d, 0x%x:0x%x:%d 0x%x/0x%x: %s, 0x%x, 0x%x\n",
        id,
        bus, dev, func,
        info->vendorId, info->deviceId,
        PciClassName(info->classCode, info->subclass, info->progIntf),
        PciRead32(id,PCI_CONFIG_HEADER_TYPE),
	IRQ0 + PciRead8(id,PCI_CONFIG_INTERRUPT_LINE)
        );
}

// ------------------------------------------------------------------------------------------------
void PciInit()
{
    kprintf("PCI Initialization\n");
    PciQueue = queue_new();
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
}

void pci_init() {
    PciInit();
}
void PciForeach(bool (*Handler)(PciDeviceInfo *dev)) {
	queue_for(de,PciQueue) {
		PciDeviceInfo *dv = (PciDeviceInfo *)de->value;
		if (Handler(dv)) {
			break;
		}
	}
}
