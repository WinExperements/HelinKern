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
typedef __SIZE_TYPE__            uintptr_t;
typedef size_t addr_t;
typedef size_t vaddr_t;
typedef size_t paddr_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef unsigned short tcflag_t;
typedef unsigned int speed_t;
typedef enum {false,true} bool;
#define OS_VERSION	"0.0.2"
#define OS_RELEASE	__DATE__
#define PANIC(msg) panic(__FILE__,__LINE__,msg)
#define assert(exp) (exp) ? 0 : panic(__FILE__,__LINE__,"Assert fail!")
static inline size_t strlen(const char* str) 
{
	size_t len = 0;
	while (*str != '\0') {
		len++;
		str++;
	}
	return len;
}
typedef uint16_t Elf32_Half;	// Unsigned half int
typedef size_t Elf32_Off;	// Unsigned offset
typedef size_t Elf32_Addr;	// Unsigned address
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
#define htonl(l)  ( (((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
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

// errors.
#define	EPERM 1		/* Not owner */
#define	ENOENT 2	/* No such file or directory */
#define	ESRCH 3		/* No such process */
#define	EINTR 4		/* Interrupted system call */
#define	EIO 5		/* I/O error */
#define	ENXIO 6		/* No such device or address */
#define	E2BIG 7		/* Arg list too long */
#define	ENOEXEC 8	/* Exec format error */
#define	EBADF 9		/* Bad file number */
#define	ECHILD 10	/* No children */
#define	EAGAIN 11	/* No more processes */
#define	ENOMEM 12	/* Not enough space */
#define	EACCES 13	/* Permission denied */
#define	EFAULT 14	/* Bad address */
#define	EBUSY 16	/* Device or resource busy */
#define	EEXIST 17	/* File exists */
#define	EXDEV 18	/* Cross-device link */
#define	ENODEV 19	/* No such device */
#define	ENOTDIR 20	/* Not a directory */
#define	EISDIR 21	/* Is a directory */
#define	EINVAL 22	/* Invalid argument */
#define	ENFILE 23	/* Too many open files in system */
#define	EMFILE 24	/* File descriptor value too large */
#define	ENOTTY 25	/* Not a character device */
#define	ETXTBSY 26	/* Text file busy */
#define	EFBIG 27	/* File too large */
#define	ENOSPC 28	/* No space left on device */
#define	ESPIPE 29	/* Illegal seek */
#define	EROFS 30	/* Read-only file system */
#define	EMLINK 31	/* Too many links */
#define	EPIPE 32	/* Broken pipe */
#define	EDOM 33		/* Mathematics argument out of domain of function */
#define	ERANGE 34	/* Result too large */
#define	ENOMSG 35	/* No message of desired type */
#define	EIDRM 36	/* Identifier removed */
#define	EDEADLK 45	/* Deadlock */
#define	ENOLCK 46	/* No lock */
#define ENOSTR 60	/* Not a stream */
#define ENODATA 61	/* No data (for no delay io) */
#define ETIME 62	/* Stream ioctl timeout */
#define ENOSR 63	/* No stream resources */
#define ENOLINK 67	/* Virtual circuit is gone */
#define EPROTO 71	/* Protocol error */
#define	EMULTIHOP 74	/* Multihop attempted */
#define EBADMSG 77	/* Bad message */
#define EFTYPE 79	/* Inappropriate file type or format */
#define ENOTUNIQ 80	/* Given log. name not unique */
#define EBADFD 81	/* f.d. invalid for this operation */
#define EREMCHG 82	/* Remote address changed */
#define ELIBACC 83	/* Can't access a needed shared lib */
#define ELIBBAD 84	/* Accessing a corrupted shared lib */
#define ELIBSCN 85	/* .lib section in a.out corrupted */
#define ELIBMAX 86	/* Attempting to link in too many libs */
#define ELIBEXEC 87	/* Attempting to exec a shared library */
#define ENOSYS 88	/* Function not implemented */
#endif

