#include<symbols.h>
#include<typedefs.h>
#include<elf.h>
#include<output.h>
#include<lib/string.h>
#include <lib/queue.h>
#include <mm/alloc.h>
#include <../userland/ksymbols.h>
Elf32_Shdr *symtab;
Elf32_Shdr *strtab;
queue_t *module_symbols;
typedef struct sym {
        char *name;
        int value;
        void *addressOfEntry;
} sym_t;
static bool useInternal = false;
Elf32_Shdr *symbols_findSection(multiboot_info_t *inf,char *name) {
	struct multiboot_elf_section_header_table t = inf->u.elf_sec;
        uint32_t addr = t.addr;
        uint32_t num = t.num;
        uint32_t shndx = t.shndx;
        Elf32_Shdr *s = (Elf32_Shdr *)addr;
        uint32_t str = s[shndx].sh_addr;
        for (uint32_t i = 0; i < num; i++) {
                uint32_t sh_name = s[i].sh_name;
                if (sh_name != 0) {
                        char *s_name = (char *)(str + sh_name);
                        if (strcmp(name,s_name)) {
                                return s + i;
                        }
                }
        }
        return NULL;
}
void symbols_init(multiboot_info_t *inf) {
	if (inf->flags == 0x454c494e) {
		// HelinBoot!
		kprintf("%s: helinboot boot protocol detected!\r\n",__func__);
		useInternal = true;
		goto initStructures;
	}
	symtab = symbols_findSection(inf,".symtab");
	strtab = symbols_findSection(inf,".strtab");
	if (symtab == NULL || strtab == NULL) {
		kprintf("No symbol/string table given by multiboot, abort.\n");
		return;
	} else if (symtab->sh_type != SHT_SYMTAB || strtab->sh_type != SHT_STRTAB) {
		kprintf("Not valid symtab/strtab!\n");
	}
initStructures:
        // initialize map for kernel drivers
        module_symbols = queue_new();
}
char *symbols_findName(int value) {
	uint32_t symbol_value = 0;
    	char *name = 0;
	if (useInternal) {
		for (int i = 0; i < KERN_SYM_COUNT; i++) {
			if (kernel_syms[i].val == value) {
				return kernel_syms[i].name;
			}
		}
		return NULL;
	}
	char *strtab_addr = (char *)strtab->sh_addr;
        Elf32_Sym *symbols = (Elf32_Sym *)symtab->sh_addr;
        for (Elf32_Word i = 0; i < symtab->sh_size / sizeof(Elf32_Sym); i++) {
                Elf32_Sym *can = symbols + i;
		            if (can->st_value >= value) {
				if ((can->st_info & STT_FUNC) != STT_FUNC) continue;
				//kprintf("can->st_value: 0x%x, st_size: 0x%x\r\n",can->st_value,can->st_size);
				// Not working.
				return 0;
			}
        }
	return "ntfnd";
}
int symbols_findValue(char *f_name) {
	if (useInternal) {
		for (int i = 0; i < KERN_SYM_COUNT; i++) {
			if (strcmp(kernel_syms[i].name,f_name)) {
				return kernel_syms[i].val;
			}
		}
		return NULL;
	}
	char *strtab_addr = (char *)strtab->sh_addr;
        Elf32_Sym *symbols = (Elf32_Sym *)symtab->sh_addr;
        for (Elf32_Word i = 0; i < symtab->sh_size / sizeof(Elf32_Sym); i++) {
                Elf32_Sym *can = symbols + i;
                uint32_t string_index = can->st_name;
                char *name = strtab_addr + string_index;
		if (strcmp(f_name,name)) {
			return can->st_value;
		}
        }
        // try to find using our modules entryes
        queue_for(element,module_symbols) {
		sym_t *sym = (sym_t *)element->value;
		if (strcmp(sym->name,f_name)) {
			return sym->value;
		}
	}
	return 0;
}
void symbols_print() {
	/*if (useInternal) {
		for (int i = 0; i < KERN_SYM_COUNT; i++) {
			kprintf("%s\r\n",kernel_syms[i].name);
		}
		return;
	}
	char *strtab_addr = (char *)strtab->sh_addr;
        Elf32_Sym *symbols = (Elf32_Sym *)symtab->sh_addr;
        for (Elf32_Word i = 0; i < symtab->sh_size / sizeof(Elf32_Sym); i++) {
                Elf32_Sym *can = symbols + i;
                uint32_t string_index = can->st_name;
                char *name = strtab_addr + string_index;
                kprintf("%s\n",name);
        }*/
        // print our modules symbols
	queue_for(element,module_symbols) {
		sym_t *sym = (sym_t *)element->value;
		kprintf("%s\r\n",sym->name);
	}
}
void symbols_registerSymbolFromModule(char *name,int val) {
        sym_t *new_sym = kmalloc(sizeof(sym_t));
	memset(new_sym,0,sizeof(sym_t));
	new_sym->name = strdup(name);
	new_sym->value = val;
	enqueue(module_symbols,new_sym);
}
