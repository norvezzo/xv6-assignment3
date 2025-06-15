#include "kernel/types.h"
#include "user/user.h"
#include "msg_protocol.h"


uint32
encode_header(uint16 index, uint16 length) {
  return ((uint32)index << 16) | (uint32)length;
}

void uitoa(int n, char *buf) {
  char tmp[10];
  int i = 0;

  // Handle 0 explicitly
  if (n == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  // Convert digits to reverse order
  while (n > 0 && i < sizeof(tmp)) {
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }

  // Reverse digits into output buffer
  int j;
  for (j = 0; j < i; j++)
    buf[j] = tmp[i - j - 1];
  buf[j] = '\0';
}

void
write_log_messages(char *buffer, uint16 index) {
  char *end = buffer + PGSIZE;
  char *ptr = buffer;
  int i = 0;

  while (1) {
    ptr = (char *)(((uint64)ptr + 3) & ~3);
    int remaining = end - ptr;
    char msg[64] = "hello from child ";  // static message
    char numbuf[10];
    uitoa(index, numbuf);              // convert msg index
    int base_len = strlen(msg);
    int num_len = strlen(numbuf);
    int total_len = base_len + num_len;

    if (total_len + 4 > remaining) {
    // Can’t fit full message, check if we can write a partial one
      if (remaining >= 5) {  // Leave room for header + at least 1 char
        int short_len = remaining - 4;
        if (__sync_val_compare_and_swap((uint32*)ptr, 0, encode_header(index, short_len)) == 0) {
          memmove(ptr + 4, msg, short_len);
        }
      }
      printf("buffer full at i=%d\n", i);
      break;
    }

    memmove(msg + base_len, numbuf, num_len);
    int len = base_len + num_len;

    if(__sync_val_compare_and_swap((uint32*)ptr, 0, encode_header(index, len)) == 0) {
      memmove(ptr + 4, msg, len);
      ptr += 4 + len;
      i++;
    }
    else {
      struct log_header *hdr = (struct log_header*)ptr;
      if (hdr->length == 0 || hdr->length > 256) {
        ptr += 4; // fallback
      } else {
        ptr += 4 + hdr->length;
      }
    } 
  }
  printf("buffer full, child done\n");
}