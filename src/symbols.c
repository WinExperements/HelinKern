#include<symbols.h>
#include<typedefs.h>
#include<elf.h>
#include<output.h>
#include<lib/string.h>
#include <lib/clist.h>
#include <mm/alloc.h>
Elf32_Shdr *symtab;
Elf32_Shdr *strtab;
clist_definition_t *module_symbols;
typedef struct sym {
        char *name;
        int value;
        void *addressOfEntry;
} sym_t;
bool symbols_nameFindDispatcher(clist_head_t *,va_list args);
void symbols_forEachDispatcher(clist_head_t *,va_list args);
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
	symtab = symbols_findSection(inf,".symtab");
	strtab = symbols_findSection(inf,".strtab");
	if (symtab == NULL || strtab == NULL) {
		kprintf("No symbol/string table given by multiboot, abort.\n");
		return;
	} else if (symtab->sh_type != SHT_SYMTAB || strtab->sh_type != SHT_STRTAB) {
		kprintf("Not valid symtab/strtab!\n");
	}
        // initialize map for kernel drivers
        module_symbols = kmalloc(sizeof(clist_definition_t));
        module_symbols->head = NULL;
        module_symbols->slot_size = sizeof(sym_t);
}
char *symbols_findName(int value) {
	uint32_t symbol_value = 0;
	char *strtab_addr = (char *)strtab->sh_addr;
        Elf32_Sym *symbols = (Elf32_Sym *)symtab->sh_addr;
        for (Elf32_Word i = 0; i < symtab->sh_size / sizeof(Elf32_Sym); i++) {
                Elf32_Sym *can = symbols + i;
		if (can->st_value > symbol_value && (int)can->st_value <= value) {
                	uint32_t string_index = can->st_name;
                	char *name = strtab_addr + string_index;
                	return name;
		}
        }
	return NULL;
}
int symbols_findValue(char *f_name) {
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
        clist_head_t *sym = clist_find(module_symbols,symbols_nameFindDispatcher,f_name);
        if (sym != NULL) {
                return ((sym_t *)sym->data)->value;
        }
	return 0;
}
void symbols_print() {
	char *strtab_addr = (char *)strtab->sh_addr;
        Elf32_Sym *symbols = (Elf32_Sym *)symtab->sh_addr;
        for (Elf32_Word i = 0; i < symtab->sh_size / sizeof(Elf32_Sym); i++) {
                Elf32_Sym *can = symbols + i;
                uint32_t string_index = can->st_name;
                char *name = strtab_addr + string_index;
                kprintf("%s\n",name);
        }
        // print our modules symbols
        clist_for_each(module_symbols,symbols_forEachDispatcher);
}
void symbols_registerSymbolFromModule(char *name,int val) {
        clist_head_t *h = clist_insert_entry_after(module_symbols,module_symbols->head);
        sym_t *sym = (sym_t *)h->data;
        sym->name = name;
        sym->value = val;
        sym->addressOfEntry = h;
}
bool symbols_nameFindDispatcher(clist_head_t *head,va_list args) {
        sym_t *sym = (sym_t *)head->data;
        char *name = va_arg(args,char *);
        return strcmp(sym->name,name);
}
void symbols_forEachDispatcher(clist_head_t *entry,va_list args) {
        sym_t *sym = (sym_t *)entry->data;
        kprintf("%s\n",sym->name);
}
