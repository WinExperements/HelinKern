/* Original code belongs to:
 * https://github.com/klange/toaruos/blob/toaru-1.x/modules/ac97.c
 *
 * I am not original developer of this driver, i just ported this driver for testing purposes only.
 * You can send pull requests to change the driver or fix bugs
 * Thanks
*/

__attribute__((section(".modname"))) char *name = "ac97";

#include <output.h>
#include <pci/driver.h>
#include <pci/registry.h>
#include <typedefs.h>

static int ctrl_id = 0;

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
    if ((info.vendorId == 0x8086) && (info.deviceId == 0x2415)) {
	    kprintf("AC97 found\n");
	    ctrl_id = id;
    }
}


static bool findAC() {
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
		return false;
	}
	return true;
}

static void module_main() {
	kprintf("Intel AC97 sound driver\n");
	if (!findAC()) {
		kprintf("Driver exit, cannot find Intel AC97 in your system\n");
		return;
	}

}


