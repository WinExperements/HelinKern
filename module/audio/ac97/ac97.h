#pragma once
#include <typedefs.h>
typedef struct _ac97_buffer {
  uint32_t address;
  uint16_t size;
  uint16_t reserved : 14;
  char last_entry : 1;
  char interrupt : 1;

}  Ac97_BufferEntry;
void ac97_init();
