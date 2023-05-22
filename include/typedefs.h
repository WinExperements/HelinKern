#ifndef TYPEDEFS_H
#define TYPEDEFS_H
// Used from my old project
#include<multiboot.h>
#include <kernel.h> // panic
#define NULL 0
#define BLACK         0
#define BLUE          0x0000FF
#define GREEN         0x00FF00
#define RED           0xFF0000
#define WHITE         0xFFFFFF
#define PRINTK_INFO 1
#define PRINTK_ERR 2
typedef char                     int8_t;
typedef unsigned char           uint8_t;
typedef short                   int16_t;
typedef unsigned short         uint16_t;
typedef int                     int32_t;
typedef unsigned int           uint32_t;
typedef long long int           int64_t;
typedef unsigned long long int uint64_t;
typedef __SIZE_TYPE__                 size_t;
typedef int s32;
typedef char s8;
typedef unsigned int            uintptr_t;
typedef uintptr_t addr_t;
typedef uintptr_t vaddr_t;
typedef uintptr_t paddr_t;
typedef enum {false,true} bool;
#define PANIC(msg) panic(__FILE__,__LINE__,msg)
#define assert(exp) (exp) ? 0 : panic(__FILE__,__LINE__,"Assert fail!")
static inline size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
typedef uint16_t Elf32_Half;	// Unsigned half int
typedef uint32_t Elf32_Off;	// Unsigned offset
typedef uint32_t Elf32_Addr;	// Unsigned address
typedef uint32_t Elf32_Word;	// Unsigned int
typedef int32_t  Elf32_Sword;	// Signed int
#define PSF_FONT_MAGIC 0x864ab572
 
typedef struct {
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} PSF_font;
typedef struct symbol
{
    char *name;
    uint32_t value;
} symbol_t;

void *memset(void *,char,int);

#endif

