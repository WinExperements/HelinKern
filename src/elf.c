#include <elf.h>
#include <arch/mmu.h>
#include <thread.h>
#include <arch/elf.h>
#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <module.h>
#include <debug.h>
static vfs_node_t *keyboard = NULL;
bool elf_check_file(Elf32_Ehdr *hdr) {
    if (!hdr) {
        kprintf("elf: given header are null\n");
        return false;
    }
    if(hdr->e_ident[EI_MAG0] != ELFMAG0) {
		kprintf("ELF Header EI_MAG0 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG1] != ELFMAG1) {
		kprintf("ELF Header EI_MAG1 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG2] != ELFMAG2) {
		kprintf("ELF Header EI_MAG2 incorrect.\n");
		return false;
	}
	if(hdr->e_ident[EI_MAG3] != ELFMAG3) {
		kprintf("ELF Header EI_MAG3 incorrect.\n");
		return false;
	}
    return true;
}
uint32_t elf_get_end_in_memory(void *elf_data) {
    uint32_t v_end;
    Elf32_Ehdr *hdr;
    Elf32_Phdr *p_entry;

    hdr = (Elf32_Ehdr *) elf_data;
    p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);

    if (!elf_check_file(elf_data))
    {
        return 0;
    }

    uint32_t result = 0;

    for (int pe = 0; pe < hdr->e_phnum; pe++, p_entry++)
    {
        //Read each entry

        if (p_entry->p_type == PT_LOAD)
        {
            v_end = p_entry->p_vaddr + p_entry->p_memsz;

            if (v_end > result)
            {
                result = v_end;
            }
        }
    }

    return result;
}
bool elf_load_file(void *addr) {
    int pe;
    Elf32_Ehdr *header = (Elf32_Ehdr *)addr;
    if (!elf_check_file(header)) {
        kprintf("%s: not a ELF file, check erros\n",__func__);
        return false;
    }
    switch(header->e_type) {
        case 1: {
            kprintf("Use insmod! Not exec\r\n");
            return false;
        }
        case 2:
        break;
       default:
        kprintf("Unknown ELF type!\r\n");
        return false;
    }
    if (!arch_elf_check_header(header)) {
        kprintf("This ELF can't be runned on this machine\r\n");
        return false;
    }
    aspace_t *space = arch_mmu_getAspace();
    arch_mmu_switch(arch_mmu_getKernelSpace());
    // create process
    process_t *prc = thread_create("ELF",header->e_entry,true);
    if (!prc) {
        kprintf("Failed to create process!\r\n");
        return false;
    }
    size_t elf_base = (size_t)addr;
    uint32_t memEnd = elf_get_end_in_memory(addr);
    // Switch to the new process memory map
    prc->aspace = arch_mmu_newAspace();
    arch_mmu_switch(prc->aspace);
    alloc_initProcess(prc,memEnd - USER_OFFSET);
    for (pe = 0; pe < header->e_phnum; pe++) {
        Elf32_Phdr *p_entry = (void *)(header->e_phoff + elf_base + pe * header->e_phentsize);
        uint32_t v_begin = p_entry->p_vaddr;
        if (p_entry->p_type == 1) {
            if (p_entry->p_memsz == 0) {
                continue; // skip
            }
            DEBUG("v_begin: 0x%x, how: %d\r\n",v_begin,p_entry->p_filesz);
            memcpy((void *) v_begin,(void *)(elf_base + p_entry->p_offset), p_entry->p_filesz);
            if (p_entry->p_memsz > p_entry->p_filesz)
            {
                char* p = (char *) p_entry->p_vaddr;
                for (int i = p_entry->p_filesz; i < (int)(p_entry->p_memsz); i++)
                {
                    p[i] = 0;
                }
            }
        }            
    }
    // back to original space
    arch_mmu_switch(space);
    if (!keyboard) keyboard = vfs_find("/dev/tty");
    vfs_node_t *ke = keyboard;
    if (ke) {
	thread_openFor(prc,ke);
	thread_openFor(prc,ke);
	thread_openFor(prc,ke); // yeah 3 times
    } 
    return true;
}
/*
    Because the kernel uses the correct implementation of the VMM and kernel heap, any module required some additional memory space to load the symbols
*/
int elf_get_module_bytes(vfs_node_t *n) {
    // read the header
    Elf32_Ehdr *header = kmalloc(sizeof(Elf32_Ehdr));
    vfs_read(n,0,sizeof(Elf32_Ehdr),header);
    if (!elf_check_file(header)) {
        kfree(header);
        return -1;
    }
    int len = (header->e_shentsize * header->e_shnum)*2048;
    kfree(header);
    return len;
}
