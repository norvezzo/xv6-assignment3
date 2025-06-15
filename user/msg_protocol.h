#ifndef MSG_PROTOCOL_H
#define MSG_PROTOCOL_H

#include "kernel/types.h"

#define PGSIZE 4096

struct log_header {
  uint16 index;
  uint16 length;
} __attribute__((packed));

uint32 encode_header(uint16 index, uint16 length);
void uitoa(int n, char *buf);
void write_log_messages(char *buffer, uint16 index);


#endif
