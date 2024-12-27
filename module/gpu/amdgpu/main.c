/* Just Simple AMD Graphics driver that uses AMD Atombios. */
/*
* Currently the driver only searches for Atom BIOS and dumps some information about it 
*/ 
#include <pci/driver.h>
#include <pci/registry.h>
#include <output.h>
#include <arch/mmu.h>
// Module name.
//__attribute__((section(".modname"))) char *name = "amdgpu";
// PCI search.
static int ctrl_id = 0;

// Structures.
typedef struct {
	uint16_t bios_magic;
	char atiMagic[10];
	uint16_t romTableBase;
} MainHeader;

typedef struct {
	uint16_t size;
	char formatRev;
	char contentRev;
} CommonHeader;

typedef struct {
	CommonHeader hdr;
	char magic[4];
	uint16_t biosRuntimeSegment;
	uint16_t protectedModeSegment;
	uint16_t configOffset;
	uint16_t crtOffset;
	uint16_t nameOffset;
	uint16_t biosCallOffset;
	uint16_t pciInitCode;
	uint16_t ioBase;
	uint16_t subsystemVendorId;
	uint16_t subsystemId;
	uint16_t pciInfoOffset;
	uint16_t commandTableOffset;
	uint16_t dataTableOffset;
	char extendedFunctionCode;
	char reserved[];
} RomTable;

static bool GPUSearch(PciDeviceInfo *dev) {
	uint32_t devType = (dev->classCode << 8) | dev->subclass;
	if (devType == PCI_DISPLAY_VGA || devType == PCI_VGA_COMPATIBLE) {
		if (dev->vendorId == VENDOR_AMD) {
			PciBar bar;
			PciGetBar(&bar,dev->baseID,0);
			if (bar.flags & PCI_BAR_IO) {
				kprintf("Failed to find device I/O base.\n");
				return false;
			}
			kprintf("AtomBIOS Should Be in 0x%x\n",bar.u.address);
			arch_mmu_mapPage(NULL,(vaddr_t)bar.u.address,(paddr_t)bar.u.address,3);
			MainHeader *hdr = (MainHeader *)bar.u.address;
			if (hdr->bios_magic != 0xAA55) {
				kprintf("Invalid magic of AtomBIOS: 0x%x at BAR0. Checking PCI ROM\n",hdr->bios_magic);
				uint32_t PciRomAddress = PciRead32(dev->baseID,PCI_CONFIG_EXPANSION_ROM);
				kprintf("Expansion ROM at 0x%x\n",PciRomAddress);
				return false;
			}
			kprintf("AtomBIOS ROM Table Base -> 0x%x\n",hdr->romTableBase);
		}
		return true;
	}
	return false;
}

void amdgpu_init() {
  kprintf("Searchig for AMD Graphics Card.....\n");
  PciForeach(GPUSearch);
}
static void module_main() {
	amdgpu_init();
}
