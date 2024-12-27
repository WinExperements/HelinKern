#include <elf.h>
#include <arch/mmu.h>
#include <thread.h>
#include <arch/elf.h>
#include <output.h>
#include <mm/alloc.h>
#include <lib/string.h>
#include <module.h>
#include <debug.h>
#include <dev.h>
#include <dev/fb.h>
#include <arch.h>
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
size_t elf_get_end_in_memory(vfs_node_t *node) {
    size_t v_end;
    Elf32_Ehdr *hdr = kmalloc(sizeof(Elf32_Ehdr));
    Elf32_Phdr *p_entry = NULL;

    vfs_read(node,0,sizeof(Elf32_Ehdr),hdr);
    //p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);
    p_entry = kmalloc(sizeof(Elf32_Phdr));
    if (!elf_check_file(hdr))
    {
        return 0;
    }

    size_t result = 0;

    for (int pe = 0; pe < hdr->e_phnum; pe++)
    {
        //Read each entry
	vfs_read(node,hdr->e_phoff + pe * hdr->e_phentsize,sizeof(Elf32_Phdr),p_entry);
        if (p_entry->p_type == PT_LOAD)
        {
            v_end = p_entry->p_vaddr + p_entry->p_memsz;

            if (v_end > result)
            {
                result = v_end;
            }
        }
	//vfs_read(node,hdr->e_phoff + pe * hdr->e_phentsize,sizeof(Elf32_Phdr),p_entry);
    }
    kfree(hdr);
    kfree(p_entry);
    //kprintf("%s: reporting %d....\r\n",__func__,result);
    return result;
}
bool elf_load_file(vfs_node_t *node,process_t *caller) {
    /*void *addr = kmalloc(node->size);
    vfs_read(node,0,node->size,addr);*/
    int pe;
    Elf32_Ehdr *header = kmalloc(sizeof(Elf32_Ehdr));
    memset(header,0,sizeof(Elf32_Ehdr));
    vfs_read(node,0,sizeof(Elf32_Ehdr),header);
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
    process_t *prc = (caller != NULL ? caller : thread_create("ELF",(void *)header->e_entry,true));
    if (!prc) {
        kprintf("Failed to create process!\r\n");
        return false;
    }
    if (caller != NULL) {
        arch_mmu_switch(prc->aspace);
        thread_recreateStack(prc,header->e_entry,true);
	      arch_mmu_switch(space);
    } else {
	    // The address space probably needeed to be created, right?
	    void *newSpace = arch_mmu_newAspace();
	    arch_mmu_duplicate(arch_mmu_getKernelSpace(),newSpace);
	    prc->aspace = newSpace;
	}
    prc->state = STATUS_CREATING;
    /*size_t elf_base = (size_t)addr; */
    size_t memEnd = elf_get_end_in_memory(node);
    // Switch to the new process memory map
    void *prc_aspace = prc->aspace; // some drivers have bugs :)
    arch_mmu_switch(prc_aspace);
    /*if (caller != NULL) {
	    fb_map();
	}*/
    if (memEnd == 0) {
	    kprintf("CAUTION! ELF End address reported AS zero! WARRNING! WARRNING!\r\n");
	    kfree(header);
	    thread_killThread(caller,0,true);
	    return false;
	}
    alloc_initProcess(prc,memEnd - USER_OFFSET);
    Elf32_Phdr *p_entry = kmalloc(sizeof(Elf32_Phdr));
    memset(p_entry,0,sizeof(Elf32_Phdr));
    for (pe = 0; pe < header->e_phnum; pe++) {
        //Elf32_Phdr *p_entry = (void *)(header->e_phoff + elf_base + pe * header->e_phentsize);
	      if (vfs_read(node,header->e_phoff + pe * header->e_phentsize,sizeof(Elf32_Phdr),p_entry) < 0) {
		        kfree(header);
		        kfree(p_entry);
		        thread_killThread(prc,10,true);
		        return 1;
	      }
        uintptr_t v_begin = p_entry->p_vaddr;
        if (p_entry->p_type == 1) {
            //kprintf("v_begin: 0x%x, how: %d, offset: %d\r\n",v_begin,p_entry->p_filesz,p_entry->p_offset);
            //memcpy((void *) v_begin,(void *)(elf_base + p_entry->p_offset), p_entry->p_filesz);
        // Initialize the address space.
        /*for (uintptr_t addr = p_entry->p_vaddr; addr <= p_entry->p_vaddr+p_entry->p_memsz; addr+=4096) {
          uintptr_t paddr = alloc_getPage();
          arch_mmu_mapPage(NULL,addr,paddr,7 | 0x00000200); // latest is a fucking PG_USED flag!
          prc->brk_next_unallocated_page_begin+=4096;
          prc->brk_end+=4096;
        }*/
	      if (vfs_read(node,p_entry->p_offset,p_entry->p_filesz,(void *)v_begin) < 0) {
		        kfree(header);
		        kfree(p_entry);
		        thread_killThread(prc,10,true);
	      }
        for (int i = p_entry->p_filesz; i < p_entry->p_memsz; ++i) {
         *(char *)(p_entry->p_vaddr + i) = 0;
        }
      }
    }
    // back to original space
    arch_mmu_switch(space);
    prc->aspace = prc_aspace;
    if (!keyboard) {
        dev_t *tt = dev_find("tty");
        if (tt != NULL) {
            keyboard = tt->devNode;
        } else {
		kprintf("serial doesn't found!\r\n");
	}
    }
    // Open stdin,stdout,stderr only for new processes that doesn't forked or something
    if (caller == NULL) {
    	vfs_node_t *ke = keyboard;
    	if (ke) {
	    	thread_openFor(prc,ke,FD_RDWR);
	    	thread_openFor(prc,ke,FD_RDWR);
	    	thread_openFor(prc,ke,FD_RDWR); // yeah 3 times
    	}
    }
    //kfree(addr);
    kfree(header);
    kfree(p_entry);
    prc->state = STATUS_RUNNING;
    /*
     * As part of substate extension, the process that will created within elf_load_file
     * will always be with substate of EXEC. As this is method how this process born.
    */
    prc->substate = SUBSTATE_EXEC;
    arch_sti();
    return true;
}
/*
    Because the kernel uses the correct implementation of the VMM and kernel heap, any module required some additional memory space to load the symbols
*/
size_t elf_get_module_bytes(vfs_node_t *n) {
    // Read the header
    Elf32_Ehdr header;
    vfs_read(n, 0, sizeof(Elf32_Ehdr), &header);

    // Check if it's a valid ELF file
    if (!elf_check_file(&header)) {
        return -1;
    }

    // Find the section header table entry
    int sh_offset = header.e_shoff;
    int sh_entry_size = header.e_shentsize;
    int sh_entry_count = header.e_shnum;

    // Calculate the size of the section headers
    int section_headers_size = sh_entry_size * sh_entry_count;

    // Find the section header string table entry
    int shstrtab_offset = sh_offset + (header.e_shstrndx * sh_entry_size);
    Elf32_Shdr shstrtab_header;
    vfs_read(n, shstrtab_offset, sizeof(Elf32_Shdr), &shstrtab_header);

    // Calculate the size of the section header string table
    int shstrtab_size = shstrtab_header.sh_size;

    // Calculate the required space to load module symbols
    int required_space = section_headers_size + shstrtab_size;

    // Multiply the required space by the factor (e.g., 2048) to ensure full loading
    int factor = 2048;
    required_space *= factor;

    return required_space;
//return 0;
}

