#include <pci/driver.h>
#include <pci/registry.h>
#include "amd_am79c973.h"
#include <output.h>
#include <arch/x86/io.h>
static int devID = 0;
static unsigned short io_base = 0;
static bool FindControllerHandler(unsigned int bus, unsigned int dev, unsigned int func) {
	unsigned int id = PCI_MAKE_ID(bus, dev, func);
	PciDeviceInfo info;
    	info.vendorId = PciRead16(id, PCI_CONFIG_VENDOR_ID);
    	if (info.vendorId == 0xffff)
    	{
        	return false;
    	}
	info.deviceId = PciRead16(id, PCI_CONFIG_DEVICE_ID);
	info.subclass = PciRead8(id, PCI_CONFIG_SUBCLASS);
    	info.classCode = PciRead8(id, PCI_CONFIG_CLASS_CODE);
	// Check device type.
	if (((info.classCode << 8) | info.subclass) == PCI_NETWORK_ETHERNET) {
		if (info.vendorId != 0x1022 || info.deviceId != 0x2000) {
			return false;
		}
		kprintf("Found ethernet controller\n");
		devID = id;
		return true;
	}
	return false;
}

/*static void module_main() {
	amdnet_init();
}*/
void amdnet_init() {
	// Find the controller.
	kprintf("AMD AM79C973 Ethernet Controller Device Driver\n");
	for (unsigned int bus = 0; bus < 256; ++bus)
    	{
        	for (unsigned int dev = 0; dev < 32; ++dev)
        	{
            		unsigned int baseId = PCI_MAKE_ID(bus, dev, 0);
            		uint8_t headerType = PciRead8(baseId, PCI_CONFIG_HEADER_TYPE);
            		unsigned int funcCount = headerType & PCI_TYPE_MULTIFUNC ? 8 : 1;
            		for (unsigned int func = 0; func < funcCount; ++func)
            		{
                		if (FindControllerHandler(bus, dev, func)) {
					bus = 257;
					break;
				}
            		}
        	}
    	}
	if (devID == 0) return; // nothing here.
	uint32_t conf = PciRead32(devID,PCI_CONFIG_COMMAND);
	conf &= 0xffff0000; // preserve status register, clear config register
	conf |= 0x5;        // set bits 0 and 2
	PciWrite32(devID,PCI_CONFIG_COMMAND,conf);
	PciBar b;
	for (int i = 0; i < 6; i++) {
		PciGetBar(&b,devID,i);
		if ((b.flags & PCI_BAR_IO) == PCI_BAR_IO) {
			kprintf("Found right BAR at %d\n",i);
			io_base = b.u.port;
			break;
		}
	}
	//io_base = PciRead32(devID,PCI_CONFIG_BAR0);
	kprintf("Device IO base: 0x%x\n",io_base);
	uint8_t mac0,mac1,mac2,mac3,mac4,mac5;
	mac0 = inb(io_base);
	mac1 = inb(io_base+1);
	mac2 = inb(io_base + 2);
	mac3 = inb(io_base + 3);
	mac4 = inb(io_base + 4);
	mac5 = inb(io_base + 5);
	kprintf("Ethernet controller MAC address: 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",mac0,mac1,mac2,mac3,mac4,mac5);
}
