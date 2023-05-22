#ifndef CLIST_H
#define CLIST_H
#include<typedefs.h>
#include<stdarg.h>
typedef struct clist_head {
    struct clist_head *prev;
    struct clist_head *next;
    void *data;
} __attribute__((packed)) clist_head_t;
typedef struct clist_definition
{
    size_t slot_size;
    struct clist_head *head;
} clist_definition_t;
typedef bool (*clist_find_callback_t)( clist_head_t* current, va_list list);
typedef void (*clist_each_callback_t)( clist_head_t* current, va_list list);
void clist_init();
clist_head_t* clist_insert_entry_after( clist_definition_t* list,  clist_head_t* pos);
clist_head_t* clist_insert_entry_before( clist_definition_t* list,  clist_head_t* pos);
void clist_delete_entry( clist_definition_t* list,  clist_head_t* entry);
clist_head_t* clist_find( clist_definition_t* list, clist_find_callback_t detector, ...);
clist_head_t* clist_find_next( clist_definition_t* list,  clist_head_t* pos, clist_find_callback_t detector, ...);
void clist_for_each( clist_definition_t* list, clist_each_callback_t cb, ...);
void clist_dump( clist_definition_t* list);
void clist_test();
int clist_size(clist_definition_t *list);
#endif
