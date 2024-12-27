#include<typedefs.h>
#include<elf.h>
#include<mm/alloc.h>
#include<module.h>
#include<output.h>
#include<lib/string.h>
#include<kernel.h>
#include<symbols.h>
#include <arch/mmu.h>
bool arch_relocSymbols(module_t *,void *);
bool module_resolve_name(module_t *mod,Elf32_Ehdr *);
bool module_resolve_dep(module_t *,Elf32_Ehdr *);
bool module_load_seg(module_t *mod,Elf32_Ehdr *);
bool module_resolve_symbols(module_t *mod,Elf32_Ehdr *);
bool module_reloc_symbols(module_t *,Elf32_Ehdr *);
module_t *mod_start;
module_t *load_module(void *address) {
    Elf32_Ehdr *header = (Elf32_Ehdr *)address;
    module_t *mod = kmalloc(sizeof(module_t));
    memset(mod,0,sizeof(module_t));
    if (!mod) {
        kprintf("Seems to your allocator broken\n");
        return NULL;
    }
    if (!elf_check_file(header)) {
        return NULL;
    }
    if (!module_resolve_name(mod,header) || !module_resolve_dep(mod,header)
        || !module_load_seg(mod,header) || !module_resolve_symbols(mod,header)
        || !module_reloc_symbols(mod,header)) {
            // Destroy any allocated by this module segments.
            module_segment_t *seg = mod->seg;
            while(seg != NULL) {
                    module_segment_t *cur = seg;
                    kfree(seg->addr);
                    seg = seg->next;
                    kfree(cur);
                }
            kfree(mod->name);
            kfree(mod);
            return NULL;
        }
    register_module(mod);
    return mod;
}
void *module_get_section_addr(module_t *mod,unsigned n) {
	module_segment_t *seg = NULL;
	for (seg = mod->seg; seg; seg = seg->next) {
		if (seg->section == n) {
			return seg->addr;
		}
	}
	return NULL;
}
bool module_resolve_name(module_t *mod,Elf32_Ehdr *header) {
    Elf32_Shdr *s;
    unsigned i;
    const char *str;
    s = (Elf32_Shdr *)((char *)header + header->e_shoff + header->e_shstrndx * header->e_shentsize);
    str = (char *)header + s->sh_offset;
    for (i = 0,s = (Elf32_Shdr *)((char *)header + header->e_shoff); i < header->e_shnum;
        i++,s = (Elf32_Shdr *)((char *)s + header->e_shentsize)) {
            if (strcmp((char *)str + s->sh_name,".modname")) {
                mod->name = strdup((char *)header + s->sh_offset);
                if (!mod->name) {
                    kprintf("Failed to set module name\n");
                    return false;
                }
                break;
            }
        }
    if (i == header->e_shnum) {
        kprintf("No module name\n");
        return false;
    }
    //kprintf("%s: setted name: %s\n",__func__,mod->name);
    return true;
}
bool module_resolve_dep(module_t *mod,Elf32_Ehdr *e) {
    Elf32_Shdr *s = NULL;
    const char *str;
    unsigned i;
    s = (Elf32_Shdr *)((char *) e + e->e_shoff + e->e_shstrndx * e->e_shentsize);
    str = (char *)e + s->sh_offset;
    for (i = 0,s = (Elf32_Shdr *)((char *)e + e->e_shoff); i < e->e_shnum;
        i++,s = (Elf32_Shdr *)((char *)s + e->e_shentsize)) {
            if (strcmp((char *)str+ s->sh_name,".moddeps")) {
                kprintf("warrning: modules deps didn't supported\n");
                return true;
            }
    }
    return true;
}
bool module_load_seg(module_t *mod,Elf32_Ehdr *e) {
    unsigned i;
    Elf32_Shdr *s;
    s = (Elf32_Shdr *)((char *) e + e->e_shoff + e->e_shstrndx * e->e_shentsize);
    const char *str = (char *)e + s->sh_offset;
    for (i = 0,s = (Elf32_Shdr *)((char *)e + e->e_shoff);
        i < e->e_shnum;
        i++,s = (Elf32_Shdr *)((char *)s+e->e_shentsize)) {
            if (s->sh_flags & (1<<1)) {
                module_segment_t *seg;
                seg = kmalloc(sizeof(module_segment_t));
                if (!seg) return false;
                if (s->sh_size) {
                    int segsize = s->sh_size;
                    void *address = (void *)e + s->sh_offset;
                    switch (s->sh_type)
                    {
                        case 8: {
                                uintptr_t wh = arch_mmu_get_io_addr(segsize);
                                if (wh == -1) {
                                        return false;
                                }
                                int pages = (segsize / 4096)+1;
                                for (int pa = 0; pa < pages; pa++) {
                                        uintptr_t adr = alloc_getPage();
                                        int off = (pa * 4096);
                                        arch_mmu_mapPage(NULL,wh+off,adr,3);
                                }
                                address = (void *)wh;
                                memset(address,0,segsize);
                        } break;
                    }
                    seg->addr = address;
                } else {
                    seg->addr = 0;
                }
                seg->size = s->sh_size;
                seg->section = i;
                seg->next = mod->seg;
                mod->seg = seg;
                // Find .text segment for debugging purpose.
                if (strcmp((char *)str + s->sh_name,".text")) {
                        mod->load_address = (uintptr_t)seg->addr;
                }
            }
        }
        return true;
}
bool module_resolve_symbols(module_t *mod,Elf32_Ehdr *e) {
	unsigned i;
	Elf32_Shdr *s;
	Elf32_Sym *sym;
	const char *str;
	Elf32_Word size,entsize;
	for (i = 0, s = (Elf32_Shdr *)((char *)e + e->e_shoff); i < e->e_shnum;
		i++,s = (Elf32_Shdr *)((char *)s + e->e_shentsize)) {
		if (s->sh_type == SHT_SYMTAB) break;
	}
	if (i == e->e_shnum) {
		kprintf("%s: failed to find symbol table in module!\n",__func__);
		return false;
	}
	mod->symtab = (Elf32_Sym *)((char *)e + s->sh_offset);
	sym = mod->symtab;
	size = s->sh_size;
	entsize = s->sh_entsize;
	s = (Elf32_Shdr *)((char *)e + e->e_shoff + e->e_shentsize * s->sh_link);
	str = (char *)e+ s->sh_offset;
	for (i = 0; i < size/entsize; i++,sym = (Elf32_Sym *)((char *)sym + entsize)) {
		uint8_t type = ELF32_ST_TYPE(sym->st_info);
		uint8_t bind = ELF32_ST_BIND(sym->st_info);
		char *name = str + sym->st_name;
		switch (type) {
			case STT_NOTYPE:
			case STT_OBJECT:
			if (sym->st_name != 0 && sym->st_shndx == 0) {
				sym->st_value += (Elf32_Addr)symbols_findValue(name);
				if (!sym->st_value) {
					kprintf("Cannot find symbol %s in kernel, abort\n",name);
					return false;
				}
			} else {
                                // Check if we need to allocate something for this symbol.
                                if (mod->symtab[i].st_shndx == SHN_COMMON) {
                                        Elf32_Sym sy = mod->symtab[i];
                                        if (sy.st_size < 4096 && sy.st_value < 512) {
                                                kprintf("modloader: maybe your symbol %s has not initialized by 0? Assuming that this symbol need to be cleared as it isn't fit in one page for size and hasn't meet minimal alignment. If you assume that mentoined symbol is actually require address then please allocate the address ourself as the in-kernel module loader currently not support this behaivor. Module loading will continue as usual.\n",name);
                                                sym->st_value = 0;
                                                continue;
                                        }
                                        int memPages = sy.st_size / 4096;
                                        uintptr_t where = arch_mmu_get_io_addr(sy.st_size);
                                        if (where == -1) {
                                                kprintf("modloader: cannot get efficient address for size %d for module %s! Load aborted.\n",sy.st_size,mod->name);
                                                return false;
                                        }
                                        paddr_t dataStart = alloc_getPages(memPages);
                                        if (dataStart == -1) {
                                                kprintf("It's looks like we out of memory\n");
                                                return false;
                                        }
                                        for (int i = 0; i < memPages; i++) {
                                                int off = (i * 4096);
                                                arch_mmu_mapPage(NULL,where+off,dataStart+off,3);
                                        }
                                        sym->st_value = where;
                                        continue;
                                }
				Elf32_Shdr *sh_hdr = (Elf32_Shdr *)(e + e->e_shoff + e->e_shentsize * mod->symtab[i].st_shndx);
				sym->st_value += sh_hdr->sh_addr;
			} break;
			case STT_FUNC:
			sym->st_value += (paddr_t) module_get_section_addr(mod,sym->st_shndx);
			if (bind != STB_LOCAL) {
				symbols_registerSymbolFromModule(name,sym->st_value);
			}
			if (strcmp(name,"module_main")) {
				//kprintf("Module init found\n");
				mod->init = (void (*)(module_t *))sym->st_value;
			} else if (strcmp(name,"module_deinit")) {
                                mod->de_init = (int (*)(module_t *))sym->st_value;
                        }
			break;
			case STT_SECTION:
			sym->st_value += (paddr_t) module_get_section_addr(mod,sym->st_shndx);
			break;
			case STT_FILE:
			sym->st_value = 0;
			break;
			default:
			return false;
			break;
		}
	}
	return true;
}
bool module_reloc_symbols(module_t *mod,Elf32_Ehdr *e) {
    	return arch_relocSymbols(mod,(void *)e);
}
void register_module(module_t *mod) {
	module_t *s = NULL;
	if (mod_start == NULL) {
		mod_start = mod;
		return;
	} else {
		s = mod_start;
		while(s->next != NULL) {
			s = s->next;
		}
		s->next = mod;
	}
}
int unregister_module(module_t *mod) {
        // Check if deinit function exist, if so, call it.
        if (mod->de_init) {
                if (!mod->de_init(mod)) {
                        kprintf("module %d deinitialization method return error. Please check logs before retrying.\n");
                        return EFAULT;
                }
        }
        // Check for symbols if we allocate some of it.

}
module_t *get_module(char *name) {
        module_t *mod = mod_start;
        while(mod != NULL) {
                if (strcmp(mod->name,name)) {
                        return mod;
                }
                mod = mod->next;
        }
        return NULL;
}
