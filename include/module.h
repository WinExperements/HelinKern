/* Module loading */
#ifndef MODULE_H
#define MODULE_H
#include <elf.h>
typedef struct module {
    char *name;
    int ref_count;
    uintptr_t load_address;
    struct module_segment *seg;
    Elf32_Sym *symtab;
    void (*init)(struct module *);
    void (*de_init)(struct module *);
    struct module *next;
} module_t;
typedef struct module_segment
{
    struct module_segment *next;
    void *addr;
    size_t size;
    unsigned section;
} module_segment_t;

module_t *load_module(void *address);
module_t *get_module(char *);
void register_module(module_t *mod);
void unregister_module(module_t *mod);
#endif
