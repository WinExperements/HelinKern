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
#include <lib/queue.h>
#include <arch.h>
#include <syscall.h>
static void *keyboard_handler(void *);
static bool shift,ctrl = false;
static void keyboard_keyHandler(char key);
static dev_t *keyboard_dev;
static uint64_t keyboard_read(struct vfs_node *node,uint64_t offset,uint64_t how,void *buf);
static bool keyboard_isReady(struct vfs_node *node);
static void readers_foreach(clist_head_t *element,va_list args);
static int setBit(int n, int k);
static int clearBit(int n, int k);
int toggleBit(int n, int k);
static int keyboardBits = 0;
static int readedI = 0;
static queue_t *keys;
extern int tty_flags;
extern queue_t *priority[6];
static char keymap[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a',
    's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c',
    'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};
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
    keyboard_dev->isReady = keyboard_isReady;
    // UPDATE 1: Add readers for each process(inspected by SOSO OS)
    // UPDATE 2: No readers more, only queues!
    keys = queue_new();
    dev_add(keyboard_dev);
}

static void *keyboard_handler(void *stack) {
    uint8_t key = inb(0x60);
    if (key < 0x80) {
	    if (key == 0x1) {
		    int (*sys_reboot)(int) = (int (*)(int))syscall_get(14);
		    sys_reboot(0xfffc04);
		    kprintf("kbdev: kernel poweroff failure\r\n");
	    }
	    if ((tty_getFlags() & FLAG_RAW) == FLAG_RAW) {
		    enqueue(keys,(void *)(int)key);
		    return stack;
	    }
        if (key == 0x2a || key == 0x36) {
            shift = true;
        } else if (key == 0x1d) {
            ctrl = true;
        } else {
            char character = keymap[key];
            if (shift) {
                // Handle shifted characters
                if ('a' <= character && character <= 'z') {
                    character -= 32; // Convert to uppercase
                } else if ('1' <= character && character <= '0') {
                    character = "!@#$%^&*()"[character - '1'];
                }
                shift = false; // Reset shift
            }
            
            if (ctrl) {
                // Handle Ctrl + key combinations
                ctrl = false; // Reset ctrl
            }
            
            keyboard_keyHandler(character);
        }
    }
    return stack;
}

static void keyboard_keyHandler(char key) {
    // Ці елементи більше не потрібні!
    enqueue(keys,(void *)(int)key);
    if ((tty_getFlags() & FLAG_ECHO) == FLAG_ECHO) {
		  output_putc(key);
    }
}
static uint64_t keyboard_read(struct vfs_node *node, uint64_t offset, uint64_t how, void *buf) {
    if (how <= 0 || buf == NULL)
        return -1;

    // Create and add reader to the structure

    char *buff = (char *)buf;
    int i = 0;

    // Enable interrupts as operations like sys_read are executed with interrupts disabled
    arch_sti();
    if (how == 1) 
    {
	    char key = (int)dequeue(keys);
	    if (key > 0) {
		    buff[0] = key;
	    	    return 1;
	    }
	    return 0;
    }
    while (i < (how - 1)) {
        while (keys->size == 0);

        char c = (int)dequeue(keys);

        if (c >= 8 && c <= 0x7e) {
            buff[i] = c;
            i++;
        }
        if (c == '\n') {
            buff[i] = '\0';
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
static bool keyboard_isReady(struct vfs_node *node) {
	return keys->size != 0;
}
