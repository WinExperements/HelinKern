#ifndef TYPEDEFS_H
#define TYPEDEFS_H
// Used from my old project
#include<multiboot.h>
#include <kernel.h> // panic
#define NULL 0
#define BLACK         0
#define BLUE          1
#define GREEN         2
#define RED           4
#define WHITE         0xF
#define PRINTK_INFO 1
#define PRINTK_ERR 2
#define min(a,b)    ((a) < (b) ? (a) : (b))
#define max(a,b)    ((a) > (b) ? (a) : (b))
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
// Grub source code
static inline uint16_t swap_bytes16(uint16_t _x)
{
   return (uint16_t) ((_x << 8) | (_x >> 8));
}

#define swap_bytes16_compile_time(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#define swap_bytes32_compile_time(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000UL) >> 24))
#define swap_bytes64_compile_time(x)	\
({ \
   uint64_t _x = (x); \
   (uint64_t) ((_x << 56) \
                    | ((_x & (uint64_t) 0xFF00ULL) << 40) \
                    | ((_x & (uint64_t) 0xFF0000ULL) << 24) \
                    | ((_x & (uint64_t) 0xFF000000ULL) << 8) \
                    | ((_x & (uint64_t) 0xFF00000000ULL) >> 8) \
                    | ((_x & (uint64_t) 0xFF0000000000ULL) >> 24) \
                    | ((_x & (uint64_t) 0xFF000000000000ULL) >> 40) \
                    | (_x >> 56)); \
})

#if (defined(__GNUC__) && (__GNUC__ > 3) && (__GNUC__ > 4 || __GNUC_MINOR__ >= 3)) || defined(__clang__)
static inline uint32_t swap_bytes32(uint32_t x)
{
	return __builtin_bswap32(x);
}

static inline uint64_t swap_bytes64(uint64_t x)
{
	return __builtin_bswap64(x);
}
#else					/* not gcc 4.3 or newer */
static inline uint32_t swap_bytes32(uint32_t _x)
{
   return ((_x << 24)
	   | ((_x & (uint32_t) 0xFF00UL) << 8)
	   | ((_x & (uint32_t) 0xFF0000UL) >> 8)
	   | (_x >> 24));
}

static inline uint64_t swap_bytes64(uint64_t _x)
{
   return ((_x << 56)
	   | ((_x & (uint64_t) 0xFF00ULL) << 40)
	   | ((_x & (uint64_t) 0xFF0000ULL) << 24)
	   | ((_x & (uint64_t) 0xFF000000ULL) << 8)
	   | ((_x & (uint64_t) 0xFF00000000ULL) >> 8)
	   | ((_x & (uint64_t) 0xFF0000000000ULL) >> 24)
	   | ((_x & (uint64_t) 0xFF000000000000ULL) >> 40)
	   | (_x >> 56));
}
#endif					/* not gcc 4.3 or newer */

#ifdef KERNEL_CPU_WORDS_BIGENDIAN
# define cpu_to_le16(x)	swap_bytes16(x)
# define cpu_to_le32(x)	swap_bytes32(x)
# define cpu_to_le64(x)	swap_bytes64(x)
# define le_to_cpu16(x)	swap_bytes16(x)
# define le_to_cpu32(x)	swap_bytes32(x)
# define le_to_cpu64(x)	swap_bytes64(x)
# define cpu_to_be16(x)	((uint16_t) (x))
# define cpu_to_be32(x)	((uint32_t) (x))
# define cpu_to_be64(x)	((uint64_t) (x))
# define be_to_cpu16(x)	((uint16_t) (x))
# define be_to_cpu32(x)	((uint32_t) (x))
# define be_to_cpu64(x)	((uint64_t) (x))
# define cpu_to_be16_compile_time(x)	((uint16_t) (x))
# define cpu_to_be32_compile_time(x)	((uint32_t) (x))
# define cpu_to_be64_compile_time(x)	((uint64_t) (x))
# define be_to_cpu64_compile_time(x)	((uint64_t) (x))
# define cpu_to_le32_compile_time(x)	swap_bytes32_compile_time(x)
# define cpu_to_le64_compile_time(x)	swap_bytes64_compile_time(x)
# define cpu_to_le16_compile_time(x)	swap_bytes16_compile_time(x)
#else /* ! WORDS_BIGENDIAN */
# define cpu_to_le16(x)	((uint16_t) (x))
# define cpu_to_le32(x)	((uint32_t) (x))
# define cpu_to_le64(x)	((uint64_t) (x))
# define le_to_cpu16(x)	((uint16_t) (x))
# define le_to_cpu32(x)	((uint32_t) (x))
# define le_to_cpu64(x)	((uint64_t) (x))
# define cpu_to_be16(x)	swap_bytes16(x)
# define cpu_to_be32(x)	swap_bytes32(x)
# define cpu_to_be64(x)	swap_bytes64(x)
# define be_to_cpu16(x)	swap_bytes16(x)
# define be_to_cpu32(x)	swap_bytes32(x)
# define be_to_cpu64(x)	swap_bytes64(x)
# define cpu_to_be16_compile_time(x)	swap_bytes16_compile_time(x)
# define cpu_to_be32_compile_time(x)	swap_bytes32_compile_time(x)
# define cpu_to_be64_compile_time(x)	swap_bytes64_compile_time(x)
# define be_to_cpu64_compile_time(x)	swap_bytes64_compile_time(x)
# define cpu_to_le16_compile_time(x)	((uint16_t) (x))
# define cpu_to_le32_compile_time(x)	((uint32_t) (x))
# define cpu_to_le64_compile_time(x)	((uint64_t) (x))

#endif /* ! WORDS_BIGENDIAN */

#endif

