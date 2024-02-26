#ifndef HELINBOOT_H
#define HELINBOOT_H

// HelinOS specific boot description block
#include <typedefs.h>

#define HELINBOOT_KERNINFO_MAGIC 0xFACB
#define HELINBOOT_MMAP_MAGIC 0xABCFA

typedef struct memMap {
	int magic; // just for test
	uint64_t begin;
	uint64_t end;
	uint64_t size;
	int type;
} MemoryMapEntry;

typedef struct modDesc {
	uint64_t begin;
	uint64_t end;
	uint64_t size;
	uint64_t cmdline;
} ModuleInfo;

// 64-bit helinOS kernel structure
typedef struct x64KrnInf {
	uint64_t magic;
	uint64_t framebufferAddress;
	uint32_t framebufferWidth;
	uint32_t framebufferHeight;
	uint32_t framebufferPitch;
	MemoryMapEntry* mmap_start;
	int memoryMapCount;
	ModuleInfo* mod;
	int moduleCount;
} kernelInfo;

#endif
