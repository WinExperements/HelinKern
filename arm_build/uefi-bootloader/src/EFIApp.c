#include "EFIApp.h"
#include "scm.h"
#include "PCIe.h"
#include "PreloaderEnvironment.h"
#include "BlBootConfiguration.h"
#include "application.h"
#include <efilib.h>


typedef enum { false,true } bool;
#define KERNBOOTDESC_MAGIC 0xFACB
#define MMAP_MAGIC 0xABCFA
#define EFI_ACPI_2_0_BOOT_GRAPHICS_RESOURCE_TABLE_SIGNATURE  SIGNATURE_32('B', 'G', 'R', 'T')

bool kernelLoaded;
bool verboseBootloaderBoot = false;
// Useless in real bootloader code currently
#define PSF_FONT_MAGIC 0x864ab572

typedef struct {
	uint32_t magic;         /* magic bytes to identify PSF */
	uint32_t version;       /* zero */
	uint32_t headersize;    /* offset of bitmaps in file, 32 */
	uint32_t flags;         /* 0 if there's no unicode table */
	uint32_t numglyph;      /* number of glyphs */
	uint32_t bytesperglyph; /* size of each glyph */
	uint32_t height;        /* height in pixels */
	uint32_t width;         /* width in pixels */
} PSF_font;

typedef struct memMap {
	int magic; // just for test
	UINT64 begin;
	UINT64 end;
	UINT64 size;
	int type;
} MemoryMapEntry;

typedef struct modDesc {
	UINT64 begin;
	UINT64 end;
	UINT64 size;
	UINT64 cmdline;
} ModuleInfo;

// 64-bit helinOS kernel structure
typedef struct x64KrnInf {
	UINT64 magic;
	UINT64 framebufferAddress;
	UINT32 framebufferWidth;
	UINT32 framebufferHeight;
	UINT32 framebufferPitch;
	MemoryMapEntry* mmap_start;
	int memoryMapCount;
	ModuleInfo* mod;
	int mouduleCount;
} kernelInfo;

typedef struct {
	UINT64  Signature;              // ACPI signature (should be 'RSD PTR ')
	UINT8   Checksum;               // ACPI 1.0 checksum
	UINT8   OemId[6];               // OEM ID
	UINT8   Revision;               // ACPI 2.0 revision
	UINT32  RsdtAddress;            // 32-bit address of the RSDT
	UINT32  Length;                 // RSDP length
	UINT64  XsdtAddress;            // 64-bit address of the XSDT (if available)
	UINT8   ExtendedChecksum;       // Checksum of entire table (including extended fields)
	UINT8   Reserved[3];            // Reserved, must be zero
} EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER;

// ACPI Description Header structure
typedef struct {
	UINT32  Signature;              // ACPI signature
	UINT32  Length;                 // Length of the table, including the header
	UINT8   Revision;               // ACPI revision number
	UINT8   Checksum;               // Checksum of entire table (including the header)
	UINT8   OemId[6];               // OEM ID
	UINT64  OemTableId;             // OEM Table ID
	UINT32  OemRevision;            // OEM Revision number
	UINT32  CreatorId;              // Vendor ID of the table creator
	UINT32  CreatorRevision;        // Revision number of the table creator
} EFI_ACPI_DESCRIPTION_HEADER;

// ACPI 2.0 Boot Graphics Resource Table (BGRT) structure
typedef struct {
	EFI_ACPI_DESCRIPTION_HEADER header;
	UINT16 version;
	UINT8 status;
	UINT8 image_type;
	UINT64 image_address;
	UINT32 image_offset_x;
	UINT32 image_offset_y;
} EFI_ACPI_2_0_BOOT_GRAPHICS_RESOURCE_TABLE;

void LoadModule(char* filePath, kernelInfo* bootInfo);
EFI_PHYSICAL_ADDRESS LoadKernel(char* kernelName, kernelInfo* kernelBootDescription);
VOID JumpToAddress(
	EFI_HANDLE ImageHandle,
	uint32_t addr,
	kernelInfo *bootInfo
)
{

	EFI_STATUS Status;
	UINTN MemMapSize = 0;
	EFI_MEMORY_DESCRIPTOR* MemMap = 0;
	UINTN MapKey = 0;
	UINTN DesSize = 0;
	UINT32 DesVersion = 0;

	/* Entry */
	VOID(*entry)(kernelInfo *) = (VOID*)addr;

	gBS->GetMemoryMap(
		&MemMapSize,
		MemMap,
		&MapKey,
		&DesSize,
		&DesVersion
	);

	/* Shutdown */
	Status = gBS->ExitBootServices(
		ImageHandle,
		MapKey
	);

	if (EFI_ERROR(Status))
	{
		Print(L"Failed to exit BS\n");
		return;
	}

	/* De-initialize */

	/* Lets go */
	entry(bootInfo);

}

BOOLEAN CheckElf32Header(Elf32_Ehdr* bl_elf_hdr)
{

	EFI_PHYSICAL_ADDRESS ElfEntryPoint;
	EFI_STATUS Status = EFI_SUCCESS;

	if (bl_elf_hdr == NULL) return FALSE;

	// Sanity check: Signature
	if (bl_elf_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		bl_elf_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		bl_elf_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		bl_elf_hdr->e_ident[EI_MAG3] != ELFMAG3)
	{
		if (verboseBootloaderBoot) Print(L"Fail: Invalid ELF magic\n");
		return FALSE;
	}

	// Sanity check: Architecture
	if (bl_elf_hdr->e_machine != EM_ARM)
	{
		if (verboseBootloaderBoot)Print(L"Fail: Not ARM architecture ELF32 file\n");
		return FALSE;
	}

	// Sanity check: exec
	if (bl_elf_hdr->e_type != ET_EXEC)
	{
		if (verboseBootloaderBoot)Print(L"Fail: Not EXEC ELF\n");
		return FALSE;
	}

	// Sanity check: entry point and size
	ElfEntryPoint = bl_elf_hdr->e_entry;
	Status = gBS->AllocatePages(
		AllocateAddress,
		EfiLoaderCode,
		1,
		&ElfEntryPoint
	);

	if (EFI_ERROR(Status))
	{
		if (verboseBootloaderBoot)Print(L"Fail: Invalid entry point\n");
		return FALSE;
	}

	// Free page allocated
	gBS->FreePages(
		ElfEntryPoint,
		1
	);

	// Sanity check: program header entries. At least one should present.
	if (bl_elf_hdr->e_phnum < 1)
	{
		if (verboseBootloaderBoot)Print(L"Fail: Less than one program header entry found\n");
		return FALSE;
	}

	return TRUE;
}

EFI_INPUT_KEY WaitForAnyKey() {
	EFI_EVENT WaitList[1];
	WaitList[0] = gST->ConIn->WaitForKey;
	UINTN Index;
	EFI_INPUT_KEY key;
	gBS->WaitForEvent(1, WaitList, &Index);
	gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
	return key;
}

// This is the actual entrypoint.
// Application entrypoint (must be set to 'efi_main' for gnu-efi crt0 compatibility)
EFI_STATUS efi_main(
	EFI_HANDLE ImageHandle,
	EFI_SYSTEM_TABLE* SystemTable
)
{
	return EFIApp_Main(ImageHandle, SystemTable, NULL);
}


EFI_STATUS PrintOrPopulateMemoryMap(EFI_STATUS populate, kernelInfo* krnInfo, void *placeAddress) {
	// get the memory map
	EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
	UINTN MemoryMapSize = 0;
	UINTN MapKey;
	UINTN DescriptorSize;
	UINT32 DescriptorVersion;
	MemoryMapEntry* entry = NULL;

	// Get the size of the memory map
	EFI_STATUS Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
	if (Status == EFI_BUFFER_TOO_SMALL) {
		// Allocate memory for the memory map
		MemoryMap = AllocatePool(MemoryMapSize);
		if (MemoryMap != NULL) {
			// Get the memory map
			Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
			if (EFI_ERROR(Status)) {
				if (verboseBootloaderBoot) Print(L"Failed to get memory map: %r\n", Status);
				return Status;
			}
			UINTN NumberOfEntries = MemoryMapSize / DescriptorSize;
			if (populate == 0) {
				if (placeAddress == NULL) {
					if (verboseBootloaderBoot)Print(L"Allocation of the MemoryMapEntry begin\r\n");
					EFI_STATUS AllocateStatus = gBS->AllocatePool(EfiLoaderData, NumberOfEntries * sizeof(MemoryMapEntry), (void**)&entry);
					if (EFI_ERROR(AllocateStatus)) {
						if (verboseBootloaderBoot)Print(L"Memory map description sector allocation error, %r\r\n");
						gBS->FreePool(MemoryMap);
						return AllocateStatus;
					}
				}
				else {
					entry = (MemoryMapEntry*)placeAddress;
					if (verboseBootloaderBoot)Print(L"Using pre-defined address of memory map entry, 0x%x, 0x%x\r\n", placeAddress, entry);
				}
			}
			krnInfo->mmap_start = entry;
			krnInfo->memoryMapCount = NumberOfEntries;
			EFI_PHYSICAL_ADDRESS LargestFreeRegionStart = 0;
			UINT64 LargestFreeRegionSize = 0;

			// Iterate through the memory map
			EFI_MEMORY_DESCRIPTOR* MemoryMapEnd = (EFI_MEMORY_DESCRIPTOR*)((UINTN)MemoryMap + MemoryMapSize);
			int num = 0;
			for (EFI_MEMORY_DESCRIPTOR* Descriptor = MemoryMap; Descriptor < MemoryMapEnd; Descriptor = (EFI_MEMORY_DESCRIPTOR*)((UINTN)Descriptor + DescriptorSize)) {
				if (populate == 0) {
					// Okay, we need to create the second memory entry that need to be passed to the kernel description region
					MemoryMapEntry ent = entry[num];
					ent.magic = MMAP_MAGIC;
					ent.begin = Descriptor->PhysicalStart;
					ent.size = Descriptor->NumberOfPages * EFI_PAGE_SIZE;
					ent.end = (UINT64)Descriptor->PhysicalStart + ent.size;
					ent.type = Descriptor->Type;
					entry[num] = ent;
					num++;
					continue;
				}
				if (Descriptor->Type == EfiConventionalMemory && Descriptor->NumberOfPages * EFI_PAGE_SIZE > LargestFreeRegionSize) {
					LargestFreeRegionStart = Descriptor->PhysicalStart;
					LargestFreeRegionSize = Descriptor->NumberOfPages * EFI_PAGE_SIZE;
				}
			}
			if (populate == 0) {
				if (verboseBootloaderBoot)Print(L"Memory map populated for kernel description. Number of entries of memory map: %d\r\n", krnInfo->memoryMapCount);
				if (verboseBootloaderBoot)Print(L"Memory Map address: 0x%lx, ended entry ID: %d\r\n", krnInfo->mmap_start, num);
				if (verboseBootloaderBoot)Print(L"Magic of start: 0x%x, first entry magic: 0x%x\r\n", entry->magic, entry[0].magic);
			}
			else {
				if (verboseBootloaderBoot)Print(L"Largest Free Memory Region: Start=0x%lx, Size=%lu bytes\n", LargestFreeRegionStart, LargestFreeRegionSize);
			}
			FreePool(MemoryMap);
		}
		else {
			Print(L"Failed to allocate memory for memory map\n");
			return EFI_OUT_OF_RESOURCES;
		}
	}
	else {
		Print(L"Failed to get memory map size: %r\n", Status);
		return Status;
	}
	return EFI_SUCCESS;
}

void FindBGRT() {
	if (verboseBootloaderBoot)Print(L"Warrning this isn't a part of bootloader process, only target device test mechanism!\r\n");
	EFI_GUID AcpiTableGuid = ACPI_TABLE_GUID;
	EFI_CONFIGURATION_TABLE* ConfigTable = gST->ConfigurationTable;
	EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER* Rsdp = NULL;
	for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
		if (CompareGuid(&(ConfigTable[i].VendorGuid), &AcpiTableGuid)) {
			Rsdp = ConfigTable[i].VendorTable;
			break;
		}
	}

	if (Rsdp == NULL) {
		if (verboseBootloaderBoot)Print(L"ACPI table not found.\n");
		return;
	}

	// Locate BGRT table
	EFI_ACPI_2_0_BOOT_GRAPHICS_RESOURCE_TABLE* Bgrt = NULL;
	EFI_ACPI_DESCRIPTION_HEADER* Table = (EFI_ACPI_DESCRIPTION_HEADER*)(UINTN)Rsdp->XsdtAddress;
	UINT64 TableEnd = (UINT64)Table + Table->Length;
	while ((UINT64)Table < TableEnd) {
		if (Table->Signature == EFI_ACPI_2_0_BOOT_GRAPHICS_RESOURCE_TABLE_SIGNATURE) {
			Bgrt = (EFI_ACPI_2_0_BOOT_GRAPHICS_RESOURCE_TABLE*)Table;
			break;
		}
		Table = (EFI_ACPI_DESCRIPTION_HEADER*)((UINT8*)Table + ((EFI_ACPI_DESCRIPTION_HEADER*)Table)->Length);
	}
	if (Bgrt == NULL) {
		if (verboseBootloaderBoot)Print(L"No BGRT found :(\r\n");
		return;
	}
	if (verboseBootloaderBoot)Print(L"BGRT Information:\n");
	if (verboseBootloaderBoot)Print(L"  Version: %d\n", Bgrt->version);
	if (verboseBootloaderBoot)Print(L"  Header Length: %d\n", Bgrt->header.Length);
}

EFI_STATUS EFIApp_Main(
	EFI_HANDLE ImageHandle,
	EFI_SYSTEM_TABLE* SystemTable,
	PBOOT_APPLICATION_PARAMETER_BLOCK BootAppParameters
)
{

	

#if defined(_GNU_EFI)
	InitializeLib(
		ImageHandle,
		SystemTable
	);
#endif
	EFI_STATUS Status = 0;
	kernelInfo* kernelBootDescription = NULL;
	Status = gBS->AllocatePool(EfiLoaderData, sizeof(kernelInfo), (void**)&kernelBootDescription);
	if (EFI_ERROR(Status)) {
		if (verboseBootloaderBoot)Print(L"Failed to allocate the kernel boot description structure. The kernel will be blind after boot.\r\n");
		kernelBootDescription = NULL;
	}
	kernelBootDescription->magic = KERNBOOTDESC_MAGIC;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
	EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	Status = gBS->LocateProtocol(&gopGuid, NULL, (void**)&gop);
	if (EFI_ERROR(Status)) {
		if (verboseBootloaderBoot)Print(L"Failed to find GOP protocol\r\n");
	}
	else {
		kernelBootDescription->framebufferAddress = gop->Mode->FrameBufferBase;
		kernelBootDescription->framebufferWidth = gop->Mode->Info->HorizontalResolution;
		kernelBootDescription->framebufferHeight = gop->Mode->Info->VerticalResolution;
		if (verboseBootloaderBoot)Print(L"Pre ELF boot kernel structure information filled\r\n");
	}
	if (verboseBootloaderBoot)Print(L"%EHelinKern BootPrv1 ARMv7 Lumia bootloader. Based on BootShim elf-loader-pre3 source%N\r\n");
	EFI_PHYSICAL_ADDRESS LkEntryPoint = LoadKernel("emmc_appsboot.mbn",kernelBootDescription);
	if (LkEntryPoint == NULL || LkEntryPoint < 0) {
		if (verboseBootloaderBoot)Print(L"Kernel Loading error!\r\n");
		return Status;
	}
	// Load emmc_appsboot.mbn
		FindBGRT();
		if (verboseBootloaderBoot) WaitForAnyKey();
		if (verboseBootloaderBoot)Print(L"HelinKern UEFI bootloader extension. Collecting info for kernelInfo structure\r\n");
		if (verboseBootloaderBoot)Print(L"Graphics side information filed. Address of FB: 0x%lx, width: %d, height: %d\r\n", kernelBootDescription->framebufferAddress, kernelBootDescription->framebufferWidth, kernelBootDescription->framebufferHeight);
		if (verboseBootloaderBoot)Print(L"Kernel boot description is located at 0x%lx\r\n", kernelBootDescription);
		/* Jump to address securely */
		JumpToAddress(
			ImageHandle,
			(uint32_t)LkEntryPoint,
			kernelBootDescription
		);
		return EFI_SUCCESS;
}

EFI_PHYSICAL_ADDRESS LoadKernel(char* kernelName,kernelInfo *kernelBootDescription) {
	if (verboseBootloaderBoot)Print(L"LoadKernel: Enter! Try to find and boot: %s\r\n", kernelName);
	EFI_STATUS Status = EFI_SUCCESS;

	UINTN NumHandles = 0;
	EFI_HANDLE* SfsHandles;

	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* EfiSfsProtocol;
	EFI_FILE_PROTOCOL* FileProtocol;
	EFI_FILE_PROTOCOL* PayloadFileProtocol;
	CHAR16* PayloadFileName = PAYLOAD_BINARY_NAME;

	EFI_PHYSICAL_ADDRESS LkEntryPoint = PAYLOAD_ENTRY_POINT_ADDR_INVALID;
	UINTN PayloadFileBufferSize;
	UINTN PayloadLoadPages;
	VOID* PayloadFileBuffer;
	VOID* PayloadLoadSec;

	EFI_FILE_INFO* PayloadFileInformation = NULL;
	UINTN PayloadFileInformationSize = 0;

	Elf32_Ehdr* PayloadElf32Ehdr = NULL;
	Elf32_Phdr* PayloadElf32Phdr = NULL;

	UINTN PayloadSectionOffset = 0;
	UINTN PayloadLength = 0;
	// Return: The kernel entry point address
	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&SfsHandles
	);

	if (EFI_ERROR(Status))
	{
		if (verboseBootloaderBoot)Print(L"Fail to locate Simple File System Handles\n");
		goto exit;
	}

	for (UINTN index = 0; index < NumHandles; index++)
	{
		Status = gBS->HandleProtocol(
			SfsHandles[index],
			&gEfiSimpleFileSystemProtocolGuid,
			(VOID**)&EfiSfsProtocol
		);

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to invoke HandleProtocol.\n");
			continue;
		}

		Status = EfiSfsProtocol->OpenVolume(
			EfiSfsProtocol,
			&FileProtocol
		);

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Fail to get file protocol handle\n");
			continue;
		}

		Status = FileProtocol->Open(
			FileProtocol,
			&PayloadFileProtocol,
			LK_BINARY_NAME,
			EFI_FILE_MODE_READ,
			EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM
		);

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to open payload image: %r\n", Status);
			continue;
		}

		// Read image and parse ELF32 file
		//Print(L"Opened payload image\n");

		Status = PayloadFileProtocol->GetInfo(
			PayloadFileProtocol,
			&gEfiFileInfoGuid,
			&PayloadFileInformationSize,
			(VOID*)PayloadFileInformation
		);

		if (Status == EFI_BUFFER_TOO_SMALL)
		{
			Status = gBS->AllocatePool(
				EfiLoaderData,
				PayloadFileInformationSize,
				&PayloadFileInformation
			);

			if (EFI_ERROR(Status))
			{
				if (verboseBootloaderBoot)Print(L"Failed to allocate pool for file info: %r\n", Status);
				goto local_cleanup;
			}

			SetMem(
				(VOID*)PayloadFileInformation,
				PayloadFileInformationSize,
				0xFF
			);

			Status = PayloadFileProtocol->GetInfo(
				PayloadFileProtocol,
				&gEfiFileInfoGuid,
				&PayloadFileInformationSize,
				(VOID*)PayloadFileInformation
			);
		}

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to stat payload image: %r\n", Status);
			goto local_cleanup;
		}

		//Print(L"Payload image size: 0x%llx\n", PayloadFileInformation->FileSize);
		if (PayloadFileInformation->FileSize > UINT32_MAX)
		{
			Print(L"Payload image is too large\n");
			goto local_cleanup_free_info;
		}

		PayloadFileBufferSize = (UINTN)PayloadFileInformation->FileSize;

		/* Allocate pool for reading file */
		Status = gBS->AllocatePool(
			EfiLoaderData,
			PayloadFileBufferSize,
			&PayloadFileBuffer
		);

		if (EFI_ERROR(Status))
		{
			Print(L"Failed to allocate pool for file: %r\n", Status);
			goto local_cleanup_free_info;
		}

		SetMem(
			PayloadFileBuffer,
			PayloadFileBufferSize,
			0xFF);

		/* Read file */
		Status = PayloadFileProtocol->Read(
			PayloadFileProtocol,
			&PayloadFileBufferSize,
			PayloadFileBuffer
		);

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to read file: %r\n", Status);
			goto local_cleanup_file_pool;
		}

		//Print(L"Payload loaded into memory at 0x%x.\n", PayloadFileBuffer);

		/* Check LK file */
		PayloadElf32Ehdr = PayloadFileBuffer;
		if (!CheckElf32Header(PayloadElf32Ehdr))
		{
			if (verboseBootloaderBoot)Print(L"Cannot load this LK image\n");
			goto local_cleanup_file_pool;
		}

		/* Check overlapping */
		if (PayloadElf32Ehdr->e_phoff < sizeof(Elf32_Ehdr))
		{
			if (verboseBootloaderBoot)Print(L"ELF header has overlapping\n");
			goto local_cleanup_file_pool;
		}

		//Print(L"Proceeded to Payload load\n");
		PayloadElf32Phdr = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadElf32Ehdr->e_phoff);
		LkEntryPoint = PayloadElf32Ehdr->e_entry;

		if (verboseBootloaderBoot)Print(L"%d sections will be inspected.\n", PayloadElf32Ehdr->e_phnum);

		/* Determine LOAD section */
		for (UINTN ph_idx = 0; ph_idx < PayloadElf32Ehdr->e_phnum; ph_idx++)
		{
			PayloadElf32Phdr = (VOID*)(((UINTN)PayloadElf32Phdr) + (ph_idx * sizeof(Elf32_Phdr)));

			/* Check if it is LOAD section */
			if (PayloadElf32Phdr->p_type != PT_LOAD)
			{
				if (verboseBootloaderBoot)Print(L"Section %d skipped because it is not LOAD, it is 0x%x\n", ph_idx, PayloadElf32Phdr->p_type);
				continue;
			}

			/* Sanity check: PA = VA, PA = entry_point, memory size = file size */
			if (PayloadElf32Phdr->p_paddr != PayloadElf32Phdr->p_vaddr)
			{
				if (verboseBootloaderBoot)Print(L"LOAD section %d skipped due to identity mapping vioaltion\n", ph_idx);
				continue;
			}

			if (PayloadElf32Phdr->p_filesz != PayloadElf32Phdr->p_memsz)
			{
				if (verboseBootloaderBoot)Print(L"%ELOAD section %d size inconsistent; use with caution%N\n", ph_idx);
			}

			if (PayloadElf32Phdr->p_paddr != LkEntryPoint)
			{
				if (verboseBootloaderBoot)Print(L"LOAD section %d skipped due to entry point violation\n", ph_idx);
				continue;
			}
			if (PayloadSectionOffset != 0) {
				if (verboseBootloaderBoot)Print(L"Loading other symbols, at: 0x%lx, size: %lu\r\n", PayloadElf32Phdr->p_paddr, PayloadElf32Phdr->p_memsz);
				UINTN PagesForSection = (PayloadElf32Phdr->p_memsz % EFI_PAGE_SIZE != 0) ?
					(PayloadElf32Phdr->p_memsz / EFI_PAGE_SIZE) + 1 :
					(PayloadElf32Phdr->p_memsz / EFI_PAGE_SIZE);
				if (verboseBootloaderBoot)Print(L"Loading %lu pages for the segment\r\n", PagesForSection);
				VOID* SegmentAddr = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadElf32Phdr->p_offset);
				SetMem((VOID*)PayloadElf32Phdr->p_paddr, PayloadElf32Phdr->p_memsz, 0xff);
				CopyMem(SegmentAddr, (VOID*)PayloadElf32Phdr->p_paddr, PayloadElf32Phdr->p_memsz);
				if (verboseBootloaderBoot)Print(L"OMAGAD! THE SEGMENT COPIED\r\n");
			}
			else {
				PayloadSectionOffset = PayloadElf32Phdr->p_offset;
				PayloadLength = PayloadElf32Phdr->p_memsz;
			}
		}

		if (PayloadSectionOffset == 0 || PayloadLength == 0)
		{
			if (verboseBootloaderBoot)Print(L"Unable to find suitable LOAD section\n");
			goto local_cleanup_file_pool;
		}

		//Print(L"ELF entry point = 0x%x\n", PayloadElf32Phdr->p_paddr);
		//Print(L"ELF offset = 0x%x\n", PayloadSectionOffset);
		//Print(L"ELF length = 0x%x\n", PayloadLength);

		PayloadLoadSec = (VOID*)(((UINTN)PayloadFileBuffer) + PayloadSectionOffset);

		/* Allocate memory for actual bootstrapping */
		PayloadLoadPages = (PayloadLength % EFI_PAGE_SIZE != 0) ?
			(PayloadLength / EFI_PAGE_SIZE) + 1 :
			(PayloadLength / EFI_PAGE_SIZE);

		//Print(L"Allocate memory at 0x%x\n", PayloadElf32Phdr->p_paddr);
		//Print(L"Allocate 0x%x pages memory\n", PayloadLoadPages);

		Status = gBS->AllocatePages(
			AllocateAddress,
			EfiLoaderCode,
			PayloadLoadPages,
			&LkEntryPoint
		);

		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to allocate memory for ELF payload\n");
			goto local_cleanup_file_pool;
		}

		/* Move LOAD section to actual location */
		SetMem(
			(VOID*)LkEntryPoint,
			PayloadLength,
			0xFF);

		CopyMem(
			(VOID*)LkEntryPoint,
			PayloadLoadSec,
			PayloadLength
		);
		//Print(L"Memory copied!\n");

		/* Jump to LOAD section entry point and never returns */
		//Print(L"\nJump to address 0x%x\n", LkEntryPoint);

		/* Ensure loader is not located too high */
		if (LkEntryPoint > UINT32_MAX)
		{
			if (verboseBootloaderBoot)Print(L"Loader located too high\n");
			Status = EFI_INVALID_PARAMETER;
			goto local_cleanup_file_pool;
		}
	local_cleanup_file_pool:
		gBS->FreePool(PayloadFileBuffer);

	local_cleanup_free_info:
		gBS->FreePool((VOID*)PayloadFileInformation);

	local_cleanup:
		Status = PayloadFileProtocol->Close(PayloadFileProtocol);
		if (EFI_ERROR(Status))
		{
			if (verboseBootloaderBoot)Print(L"Failed to close Payload image: %r\n", Status);
		}

		break;
	}
	void* placeAddress = (void*)(LkEntryPoint + PayloadLength + 100);
	if (verboseBootloaderBoot)Print(L"Memory map will be placed at 0x%x\r\n", NULL);
	if (EFI_ERROR(PrintOrPopulateMemoryMap(EFI_SUCCESS, kernelBootDescription, NULL))) {
		if (verboseBootloaderBoot)Print(L"%UFailed to place memory map information to the end address address: 0x%x, error: %r\r\n", placeAddress);
		gBS->Stall(SECONDS_TO_MICROSECONDS(5));
		return Status;
	}
	return (EFI_PHYSICAL_ADDRESS)LkEntryPoint;
exit:
	// If something fails, give 5 seconds to user inspect what happened
	gBS->Stall(SECONDS_TO_MICROSECONDS(5));
	return Status;
}

void LoadModule(char* filePath, kernelInfo* bootInfo) {
	// Yeah, we can pass the fucking modules to the kernel
	// First, try to open the module itself
	
}