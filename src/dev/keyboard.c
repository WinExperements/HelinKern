#include <dev/keyboard.h>
#include <arch/x86/io.h>
#include <arch/x86/idt.h>
#include <kernel.h>
#include <output.h>
#include <dev.h>
#include <mm/alloc.h>
#include <lib/clist.h>
#include <debug.h>
#include <tty.h> // TTY!!!
static void *keyboard_handler(void *);
static bool shift,ctrl = false;
static void keyboard_keyHandler(char key);
static dev_t *keyboard_dev;
static int keyboard_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static void readers_foreach(clist_head_t *element,va_list args);
static int setBit(int n, int k);
static int clearBit(int n, int k);
int toggleBit(int n, int k);
static int keyboardBits = 0;
static int readedI = 0;
struct keyboard_reader {
    bool hasKey;
    char key;
};
static clist_definition_t *readers;
extern int tty_flags;
void keyboard_init() {
    outb(0x64,0xAD); // disable first PS/2 port
	outb(0x64,0xA7); // disable second PS/2 port(if supported)
	while(inb(0x64) & 0) {
	}
	// re-enable disabled ports
	outb(0x64,0xAE);
	outb(0x64,0xA8);
	// Do self-test?
	// Used the BOCHS source code
	
    interrupt_add(33,keyboard_handler);
    //interrupt_add(IRQ12,keyboard_handler);
    // Register keyboard in devfs
    keyboard_dev = kmalloc(sizeof(dev_t));
    memset(keyboard_dev,0,sizeof(dev_t));
    keyboard_dev->name = "keyboard";
    keyboard_dev->buffer_sizeMax = 100;
    keyboard_dev->read = keyboard_read;
    // UPDATE 1: Add readers for each process(inspected by SOSO OS)
    readers = kmalloc(sizeof(clist_definition_t));
    memset(readers,0,sizeof(clist_definition_t));
    readers->slot_size = sizeof(struct keyboard_reader);
    dev_add(keyboard_dev);
}
static void *keyboard_handler(void *stack) {
    uint8_t key = inb(0x60);
	if (key < 0x80)
	{
        //printf("key: %x\n",key);
		switch(key)
        {
            case 0x02: keyboard_keyHandler('1'); break;
            case 0x03: keyboard_keyHandler('2'); break;
            case 0x04: keyboard_keyHandler('3'); break;
            case 0x05: keyboard_keyHandler('4'); break;
            case 0x06: keyboard_keyHandler('5'); break;
            case 0x07: keyboard_keyHandler('6'); break;
            case 0x08: {
                if (!shift) {
                    keyboard_keyHandler('7');
                } else {
                    keyboard_keyHandler('&');
                }
            } break;
            case 0x09: keyboard_keyHandler('8'); break;
            case 0x0A: keyboard_keyHandler('9'); break;
            case 0x0B: keyboard_keyHandler('0'); break;
	    case 0x1: {
			      kprintf("Killing current process...");
			      process_t *prc = thread_getThread(thread_getCurrent());
			      if (prc == NULL) {
				      kprintf("process not found\n");
				      break;
				} else if (prc->pid == 0) {
					kprintf("Can't kill kswapper\n");
				}
				kprintf("done\n"); // actuall thread_killThread do reschedule via arch_reschedule
			      thread_killThread(prc,1);
			} break;

            case 0x10: keyboard_keyHandler('q'); break;
            case 0x11: keyboard_keyHandler('w'); break;
            case 0x12: keyboard_keyHandler('e'); break;
            case 0x13: keyboard_keyHandler('r'); break;
            case 0x14: keyboard_keyHandler('t'); break;
            case 0x15: keyboard_keyHandler('y'); break;
            case 0x16: keyboard_keyHandler('u'); break;
            case 0x17: keyboard_keyHandler('i'); break;
            case 0x18: keyboard_keyHandler('o'); break;
            case 0x19: keyboard_keyHandler('p'); break;

            case 0x1E: keyboard_keyHandler('a'); break;
            case 0x1F: keyboard_keyHandler('s'); break;
            case 0x20: keyboard_keyHandler('d'); break;
            case 0x21: keyboard_keyHandler('f'); break;
            case 0x22: keyboard_keyHandler('g'); break;
            case 0x23: keyboard_keyHandler('h'); break;
            case 0x24: keyboard_keyHandler('j'); break;
            case 0x25: keyboard_keyHandler('k'); break;
            case 0x26: keyboard_keyHandler('l'); break;

            case 0x2C: keyboard_keyHandler('z'); break;
            case 0x2D: keyboard_keyHandler('x'); break;
            case 0x2E: {
                keyboard_keyHandler('c');
            } break;
            case 0x2F: keyboard_keyHandler('v'); break;
            case 0x30: keyboard_keyHandler('b'); break;
            case 0x31: keyboard_keyHandler('n'); break;
            case 0x32: keyboard_keyHandler('m'); break;
            case 0x33: keyboard_keyHandler(','); break;
            case 0x34: keyboard_keyHandler('.'); break;
            case 0x35: keyboard_keyHandler('/'); break;

            case 0x1C: keyboard_keyHandler('\n'); break;
            case 0x39: keyboard_keyHandler(' '); break;
            case 0xe: {
                keyboard_keyHandler('\b');
            } break;
            case 0x2b: keyboard_keyHandler('\\'); break;
            case 0x2a: {shift = !shift;} break;
            case 0x1d: {ctrl = !ctrl;} break;
            case 0x3a: {
                // Toogle the bit
                toggleBit(keyboardBits,2);
                // Send it to the PS/2 controller
                outb(0x60,keyboardBits);
                outb(0x60,0xED);
                // Wait for keyboard ACK
                int timeout = 1000;
                while(inb(0x64) != 0xFA) {
                    if (timeout <= 0) break;
                    timeout--;
                }
            } break;
            default:
            {
		keyboard_keyHandler(key);
                //kprintf("Unhandled key: 0x%x\r\n",key);
                break;
            }
	}
    }
    return stack;
}
static void readers_foreach(clist_head_t *element,va_list args) {
    // Цей проект буде містити коментарі на двої мовах, обидві мови я знаю, на данний момент часу, всі вони написані одною людиною.
    struct keyboard_reader *reader = element->data;
    reader->hasKey = true;
    reader->key = va_arg(args,int);
}
static void keyboard_keyHandler(char key) {
    // Ці елементи більше не потрібні!
    clist_for_each(readers,readers_foreach,key);
    if (tty_flags & FLAG_ECHO) {
		kprintf("%c",key);
	}
}
static int keyboard_read(struct vfs_node *node, uint64_t offset, uint64_t how, void *buf) {
    if (how <= 0 || buf == NULL)
        return -1;

    // Create and add reader to the structure
    clist_head_t *d = clist_insert_entry_after(readers, readers->head);
    struct keyboard_reader *reader = d->data;

    char *buff = (char *)buf;
    int i = 0;

    // Enable interrupts as operations like sys_read are executed with interrupts disabled
    arch_sti();

    while (i < (how - 1)) {
        reader->hasKey = false;
        while (!reader->hasKey)
            ;

        char c = reader->key;

        if (c >= 8 && c <= 0x7e) {
            buff[i] = c;
            i++;
        }
        if (c == '\n') {
            buff[i] = '\0';
            clist_delete_entry(readers, d);
            readedI = i;
            return i;
        }
        if (c == '\b') {
            if (i > 0) {
                i-=2;
                buff[i] = '\0';
            }
        }
    }

    clist_delete_entry(readers, d);
    readedI = i;
    return i;
}

static int setBit(int n, int k)
{
    return (n|(1<<(k-1)));
}
// Function to clear the kth bit of n
static int clearBit(int n, int k)
{
    return (n & (~(1 << (k - 1))));
}
int toggleBit(int n, int k)
{
    return (n^(1<<(k-1)));
}
