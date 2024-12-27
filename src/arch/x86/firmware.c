#include <typedefs.h>
#include <lib/string.h>
#include <arch/x86/firmware.h>
#include <output.h>
#include <arch/mmu.h>
typedef struct _smbios_entry {
  char anchor[4];
  uint8_t checksum;
  uint8_t entrySize;
  uint8_t majorVer;
  uint8_t minorVer;
  uint16_t structMaxSize;
  uint8_t revision;
  char formatData[5];
  char intermidAnchor[5];
  uint8_t intermiChecksum;
  uint16_t tableSize;
  uint32_t tableAddress;
  uint16_t strnum;
  char bcdRev;
} __attribute__((packed)) SMBIOSEntryPoint;
typedef struct _smbios_header {
  uint8_t type;
  uint8_t size;
  uint16_t handle;
} __attribute__((packed)) SMBIOSHeader;
typedef struct _smbios_bios {
    uint8_t type;
    uint8_t length;
    uint16_t handle; //4 bytes

    uint8_t vendor;
    uint8_t version;
    uint16_t start_address_segment; //8 bytes
    uint8_t release_date;
    uint8_t rom_size; //10bytes
    uint64_t characteristics; //14bytes

    uint8_t characteristics_extension_bytes[2]; //16bytes
    uint8_t system_bios_major_release;
    uint8_t system_bios_minor_release;
    uint8_t embedded_controller_major_release;
    uint8_t embedded_controller_minor_release; //20bytes 
} SMBIOS_BiosInfo;
typedef struct _smbios_processor {
  uint8_t type;                      //Common: 4 bytes
    uint8_t length;
    uint16_t handle;                   

    uint8_t socket_designation;        //2.0+   18 bytes
    uint8_t processor_type;
    uint8_t processor_family;
    uint8_t processor_manufacturer;
    uint64_t processor_id;
    uint8_t processor_version;
    uint8_t voltage;
    uint16_t external_clock;
    uint16_t max_speed;
    uint16_t current_speed;
    uint8_t status;
    uint8_t processor_upgrade;          

    uint16_t l1_cache_handle;           //2.1+   6 bytes
    uint16_t l2_cache_handle;
    uint16_t l3_cache_handle;           

    uint8_t serial_number;              //2.3+   3 bytes
    uint8_t asset_tag;
    uint8_t part_number;                
} __attribute__((packed)) SMBIOS_ProcessorInfo;
static SMBIOSEntryPoint *entry;
void x86_smbios_init() {
  char *start = (char *)0x000F0000;
  int size,i;
  uint8_t checksum;
  while (start <= (char *)0x000FFFFF) {
    if (!memcmp(start,"_SM_",4)) {
      size = start[5];
      checksum = 0;
      for (int i = 0; i < size; i++) {
        checksum += start[i];
      }
      if (checksum == 0) {
        kprintf("%s: found SMBIOS\n",__func__);
        break;
      }
    }
    start += 16;
  }
  if ((unsigned int)start == 0x00100000) {
    kprintf("%s: SMBIOS not found. Disabling this feature\n",__func__);
    return;
  }
  entry = (SMBIOSEntryPoint *)start;
  for (int page = 0; page < 3; page++) {
    uintptr_t st = entry->tableAddress+(page*4096);
    arch_mmu_mapPage(NULL,st,st,3);
  }
  kprintf("%s: SMBIOS version: %d.%d\n",__func__,entry->majorVer,entry->minorVer);
}
size_t x86_smbios_getStructSize(void *first_addr) {
    char * addr = first_addr;

    while (*addr != '\0' || *(addr+1) != '\0') {
        addr++;
    }

    return ((uintptr_t)(addr+2) - (uintptr_t)first_addr);
}
char *x86_smbios_getString(SMBIOSHeader *hdr,uint8_t stringID) {
  char *r = (char *)hdr + hdr->size;
  for (int i = 0; i < (stringID-1); i++) {
    r += strlen(r) + 1;
  }
  return r;
}
void x86_smbios_printInfo(int type) {
  SMBIOSHeader *hdr = (SMBIOSHeader *)entry->tableAddress;
  int i = 0;
  for (i = 0; i < entry->strnum; i++) {
    if (hdr->type != type)  {
      hdr = (SMBIOSHeader *)((uintptr_t)hdr+hdr->size);
      hdr = (SMBIOSHeader *)((uintptr_t)hdr+(uintptr_t)x86_smbios_getStructSize(hdr));
      continue;
    } else {
      kprintf("%s: got required structure!\n",__func__);
      break;
    }
  }
  if (i == entry->strnum) {
    kprintf("SMBIOS doesn't have required structure\n");
    return;
  }
  switch(type) {
    case 0: {
      SMBIOS_BiosInfo *bios = (SMBIOS_BiosInfo *) entry->tableAddress;
      const char *strtab = (const char *) hdr + hdr->size;
      kprintf("Firmware Vendor: %s. Firmware version: %s\n",x86_smbios_getString(hdr,bios->vendor),x86_smbios_getString(hdr,bios->version));
      kprintf("Firmware release date: %s\n",x86_smbios_getString(hdr,bios->release_date));
    } break;
    case 4: {
      SMBIOS_ProcessorInfo *cpu = (SMBIOS_ProcessorInfo *)entry->tableAddress;
      kprintf("Processor Vendor: %s\n",x86_smbios_getString(hdr,cpu->processor_manufacturer));
      kprintf("Processor version: %s\n",x86_smbios_getString(hdr,cpu->processor_version));
      int maxSpeed = cpu->max_speed/1000;
      kprintf("Max speed: %d GHz.\n",maxSpeed);
      kprintf("Boot speed: %d MHz.\n",cpu->current_speed);
      kprintf("Processor serial: %s\n",x86_smbios_getString(hdr,cpu->serial_number));
    } break;
  }
}
