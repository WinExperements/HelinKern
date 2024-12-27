#ifndef _SYS_ENDIAN_H
#define _SYS_ENDIAN_H
#include <machine/endian.h>
#  define htobe16(x) __bswap16 (x)
#  define htole16(x) ((__uint16_t)(x))
#  define be16toh(x) __bswap16 (x)
#  define le16toh(x)  ((__uint16_t)(x))

#  define htobe32(x) __bswap32 (x)
#  define htole32(x)  ((__uint32_t)(x))
#  define be32toh(x) __bswap32 (x)
#  define le32toh(x)  ((__uint32_t)(x))

#  define htobe64(x) __bswap64 (x)
#  define htole64(x)  ((__uint64_t)(x))
#  define be64toh(x) __bswap64 (x)
#  define le64toh(x)  ((__uint64_t)(x))
#endif
