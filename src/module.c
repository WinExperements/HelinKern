#include<typedefs.h>
#include<elf.h>
#include<mm/alloc.h>
#include<module.h>
#include<output.h>
#include<lib/string.h>
#include<kernel.h>
#include<symbols.h>
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
            return NULL;
        }
    register_module(mod);
    return mod;
}
void *module_get_section_addr(module_t *mod,unsigned n) {
	module_segment_t *seg = NULL;
	for (seg = mod->seg; seg; seg = seg->next) {
		if (seg->section == n) {
           		if (n == 1) {
				mod->load_address = (uintptr_t)seg->addr;
			}
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
    for (i = 0,s = (Elf32_Shdr *)((char *)e + e->e_shoff);
        i < e->e_shnum;
        i++,s = (Elf32_Shdr *)((char *)s+e->e_shentsize)) {
            if (s->sh_flags & (1<<1)) {
                module_segment_t *seg;
                seg = kmalloc(sizeof(module_segment_t));
                if (!seg) return false;
                if (s->sh_size) {
                    void *address = kmalloc(s->sh_size+s->sh_addralign) + s->sh_addralign;
                    if (!address) {
                        kfree(seg);
                        return false;
                    }
                    switch (s->sh_type)
                    {
                        case 1: 
                        memcpy(address,(char *)e + s->sh_offset,s->sh_size);
                        break;
                        case 8:
                        memset(address,0,s->sh_size);
                        break;
                    }
                    seg->addr = address;
                } else {
                    seg->addr = 0;
                }
                seg->size = s->sh_size;
                seg->section = i;
                seg->next = mod->seg;
                mod->seg = seg;
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
		kprintf("%s: failed to find symbol table in module!\n");
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

