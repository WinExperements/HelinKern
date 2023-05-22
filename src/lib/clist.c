#include<lib/clist.h>
#include<mm/alloc.h>
#include<output.h>
#include<stdarg.h>
#include<kernel.h>
clist_head_t* clist_insert_entry_after(clist_definition_t *list,clist_head_t* pos)
{
    /* allocate memory */
    clist_head_t* entry;
    size_t addr = (size_t)kmalloc(list->slot_size);
    entry = (clist_head_t*)kmalloc(sizeof(clist_head_t));
    entry->data = (void*)addr;
    if (list->head == NULL) {
        /* insert first entry */
        entry->next = entry;
        entry->prev = entry;
        list->head = entry;
    } else {
        clist_head_t* prev = pos;
        clist_head_t* next = pos->next;
        entry->prev = prev;
        entry->next = next;
        if (prev != NULL) {
            prev->next = entry;
        }
        if (next != NULL) {
            next->prev = entry;
        }
    }
    return entry;
}
clist_head_t* clist_insert_entry_before(clist_definition_t* list,clist_head_t* pos)
{

    /* allocate memory */
    clist_head_t* entry;
    size_t addr = (size_t)kmalloc(list->slot_size);
    entry = (clist_head_t*)addr;
    entry->data = (void*)addr;

    if (list->head == NULL) {
        /* insert first entry */
        entry->next = entry;
        entry->prev = entry;
        list->head = entry;
    } else {
        /* insert next entry between pos prev and pos */
        clist_head_t* prev = pos->prev;
        clist_head_t* next = pos;
        entry->prev = prev;
        entry->next = next;
        if (prev != NULL) {
            prev->next = entry;
        }
        if (next != NULL) {
            next->prev = entry;
        }
    }

    return entry;
}
void clist_delete_entry(clist_definition_t* list,clist_head_t* entry)
{
    clist_head_t* prev = entry->prev;
    clist_head_t* next = entry->next;
    prev->next = next;
    next->prev = prev;
    if (list->head == entry) {
        list->head = entry->next != entry ? entry->next : NULL;
    }
    kfree(entry);
}
clist_head_t* clist_find(clist_definition_t* list, clist_find_callback_t detector, ...)
{
    clist_head_t* current;

    for (current = list->head; current != NULL; current = current->next) {
        va_list args;
        va_start(args, detector);

        if (detector(current, args)) {
            return current;
        }

        if (current->next == list->head) {
            break;
        }
    }

    return NULL;
}
clist_head_t* clist_find_next(clist_definition_t* list, clist_head_t* pos, clist_find_callback_t detector, ...)
{
    clist_head_t* current;

    for (current = pos->next; current != NULL && current != pos; current = current->next) {
        va_list args;
        va_start(args, detector);

        if (detector(current, args)) {
            return current;
        }
    }

    if (pos != NULL) {
        va_list args;
        va_start(args, detector);
        if (detector(current, args)) {
            return current;
        }
    }
    return NULL;
}
void clist_for_each(clist_definition_t* list, clist_each_callback_t cb, ...) {
    clist_head_t* current;

    for (current = list->head; current != NULL; current = current->next) {
        va_list args;
        va_start(args, cb);

        cb(current, args);

        if (current->next == list->head) {
            break;
        }
    }
}
void clist_dump(clist_definition_t* list)
{
    kprintf("-- cyclic list dump\n");
    clist_head_t* current;
    for (current = list->head; current != NULL; current = current->next) {
        kprintf("  this=%X prev=%X next=%X\n", current, current->prev, current->next);
        if (current->next == list->head) {
            break;
        }
    }
}
void clist_test() {
    clist_definition_t list = {
        .head = NULL,
        .slot_size = sizeof(clist_head_t)
    };

    clist_head_t* entry1;
    clist_head_t* entry2;
    clist_head_t* entry3;
    clist_head_t* entry4;

    /* create first entry (1) */
    entry1 = clist_insert_entry_after(&list, NULL);
    assert(entry1 != NULL);
    assert(list.head == entry1);
    /* create second entry (2, 1) */
    entry2 = clist_insert_entry_before(&list, entry1);
    assert(entry2 != NULL);
    assert(list.head == entry1);
    /* create third entry (2, 1, 3) */
    entry3 = clist_insert_entry_after(&list, entry1);
    assert(entry3 != NULL);
    assert(list.head == entry1);
    /* create fourth entry (2, 1, 3, 4) */
    entry4 = clist_insert_entry_after(&list, entry3);
    assert(entry4 != NULL);
    assert(list.head == entry1);

    /* delete first entry (2, 3, 4) */
    clist_delete_entry(&list, entry1);
    assert(list.head == entry3);
    /* delete fourth entry (2, 3) */
    clist_delete_entry(&list, entry4);
    assert(list.head == entry3);
    /* delete third entry (2) */
    clist_delete_entry(&list, entry3);
    assert(list.head == entry2);
    /* delete second entry () */
    clist_delete_entry(&list, entry2);
    assert(list.head == NULL);
}
int clist_size(clist_definition_t *list) {
    int size = 0;
    clist_head_t* current;
    for (current = list->head; current != NULL; current = current->next) {
        size++;
        if (current->next == list->head) {
            break;
        }
    }
    return size;
}
