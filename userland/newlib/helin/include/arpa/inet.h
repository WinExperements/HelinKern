#ifndef ARPA_INET_H
#define ARPA_INET_H

#include <stdint.h>

__BEGIN_DECLS

uint32_t htonl(uint32_t);
uint16_t htons(uint16_t);
uint32_t ntohl(uint32_t);
uint16_t ntohs(uint16_t);

#endif
