#include <dev/socket.h>
#include <output.h>
#include <lib/clist.h> // yeah clists!
#include <lib/string.h> // memset

// Socket implementation

static bool domain_find(clist_head_t *head,va_list args);
static clist_definition_t *sockets;
typedef struct _entry {
	int domain;
	bool (*create)(Socket *socket);
} entry;

void socket_init() {
	sockets = (clist_definition_t *)kmalloc(sizeof(clist_definition_t));
	memset(sockets,0,sizeof(clist_definition_t));
	sockets->slot_size = sizeof(entry);
	kprintf("socket: initialized\n");
}

void socket_add(int domain,bool (*create)(Socket *socket)) {
	if (create == NULL) return;
	void *a = clist_find(sockets,domain_find,domain);
	if (a != NULL) {
		// We already registred it before
		return;
	}
	entry *en = (entry *)kmalloc(sizeof(entry));
	memset(en,0,sizeof(entry)); // always clear memory
	en->domain = domain;
	en->create = create;
	// Insert our entry into list
	clist_head_t *ent = clist_insert_entry_after(sockets,sockets->head);
	ent->data = en;
	// All done
}

bool socket_create(int domain,Socket *socket) {
	if (socket == NULL) return false;
	entry *en = clist_find(sockets,domain_find,domain)->data;
	if (en == NULL) return false;
	return en->create(socket);
}

static bool domain_find(clist_head_t *head,va_list args) {
	int domain = va_arg(args,int);
	entry *en = head->data;
	return en->domain == domain;
}
