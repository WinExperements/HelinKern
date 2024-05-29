/* Just Simple AMD Graphics driver that uses AMD Atombios. */
/*
* Currently the driver only searches for Atom BIOS and dumps some information about it 
*/ 
#include <pci/driver.h>
#include <pci/registry.h>
#include <output.h>

// Module name.
__attribute__((section(".modname"))) char *name = "amdgpu";
// PCI search.
static int ctrl_id = 0;

static bool PciVisit(unsigned int bus, unsigned int dev, unsigned int func)
{
    unsigned int id = PCI_MAKE_ID(bus, dev, func);

    PciDeviceInfo info;
    info.vendorId = PciRead16(id, PCI_CONFIG_VENDOR_ID);
    if (info.vendorId == 0xffff)
    {
        return false;
    }

    info.deviceId = PciRead16(id, PCI_CONFIG_DEVICE_ID);
    info.progIntf = PciRead8(id, PCI_CONFIG_PROG_INTF);
    info.subclass = PciRead8(id, PCI_CONFIG_SUBCLASS);
    info.classCode = PciRead8(id, PCI_CONFIG_CLASS_CODE);
    if (((info.classCode << 8) | info.subclass) == PCI_DISPLAY_VGA) {
      kprintf("Device vendor: 0x%x\r\n",info.vendorId);
      ctrl_id = id;
      return true;
  }
  return false;
}


static void module_main() {
  kprintf("Searchig for AMD Graphics Card.....");
  for (unsigned int bus = 0; bus < 256; bus++) {
    for (unsigned int dev = 0; dev < 32; dev++) {
      unsigned int baseId = PCI_MAKE_ID(bus, dev, 0);
      uint8_t headerType = PciRead8(baseId, PCI_CONFIG_HEADER_TYPE);
      unsigned int funcCount = headerType & PCI_TYPE_MULTIFUNC ? 8 : 1;
      for (unsigned int func = 0; func < funcCount; ++func)
      {
        if (PciVisit(bus, dev, func)) {
          goto found;
        }
      }
    }
    kprintf("Device not found, stop\r\n");
    return;
  found:
    unsigned int BAR0 = PciRead32(ctrl_id,PCI_CONFIG_BAR0);
    kprintf("AtomBIOS must be right in 0x%x\r\n",BAR0);
  }
}
