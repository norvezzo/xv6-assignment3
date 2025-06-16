#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PGSIZE 4096
#define NCHILDREN 8
#define HEADER_COMPLETE_BIT (1 << 31)

struct log_header {
  uint16 index;
  uint16 length;
} __attribute__((packed));

uint32
encode_header(uint16 index, uint16 length, int complete) {
  uint32 base = ((uint32)index << 16) | (uint32)length;
  return complete ? (base | HEADER_COMPLETE_BIT) : base;
}

int
is_header_complete(uint32 hdr) {
  return (hdr & HEADER_COMPLETE_BIT) != 0;
}

uint16
get_header_index(uint32 hdr) {
  return (hdr >> 16) & 0x7FFF; // mask off complete bit
}

uint16
get_header_length(uint32 hdr) {
  return hdr & 0xFFFF;
}

void uitoa(int n, char *buf) {
  char tmp[10];
  int i = 0;

  if (n == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return;
  }

  while (n > 0 && i < sizeof(tmp)) {
    tmp[i++] = '0' + (n % 10);
    n /= 10;
  }

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

  char *messages[] = {
    "i'm child ",
    "short message by child ",
    "medium length message by child ",
    "this is a much longer message written by child "
  };

  while (1) {
    ptr = (char *)(((uint64)ptr + 3) & ~3);
    int remaining = end - ptr;
    int variation = i % 4;
    char *msg = messages[variation];
    int base_len = strlen(msg);

    char numbuf[10];
    uitoa(index, numbuf);
    int num_len = strlen(numbuf);
    int total_len = base_len + num_len;

    if (total_len + 4 > remaining) {
      if (remaining >= 5) {
        int short_len = remaining - 4;
        if (__sync_val_compare_and_swap((uint32*)ptr, 0, encode_header(index, short_len, 0)) == 0) {
          memmove(ptr + 4, msg, short_len);
          __sync_synchronize(); // memory barrier
          __sync_lock_test_and_set((uint32*)ptr, encode_header(index, short_len, 1));
          __sync_synchronize();
          //*(uint32*)ptr = encode_header(index, short_len, 1);
        }
        else 
          printf("child %d: failed to reserve partial slot at %p\n", index, ptr);
      }
      break;
    }

    if (__sync_val_compare_and_swap((uint32*)ptr, 0, encode_header(index, total_len, 0)) == 0) {
      memmove(ptr + 4, msg, base_len);
      memmove(ptr + 4 + base_len, numbuf, num_len);
      __sync_synchronize(); // memory barrier
      __sync_lock_test_and_set((uint32*)ptr, encode_header(index, total_len, 1));
      __sync_synchronize();
      //*(uint32*)ptr = encode_header(index, total_len, 1);
      ptr += 4 + total_len;
      i++;
      //sleep(1);
    } else {
      uint32 hdr = *(uint32*)ptr;
      uint16 skip = get_header_length(hdr);
      ptr += 4 + skip;
    }
  }
}

int
main(int argc, char *argv[]) {
  int parent_pid = getpid();
  void *shared = malloc(PGSIZE);
  memset(shared, 0xAA, PGSIZE);
  __sync_synchronize();

  if (!shared) {
    printf("malloc failed\n");
    exit(1);
  }

  memset(shared, 0, PGSIZE);
  uint64 shared_va = (uint64)shared;

  for (int i = 0; i < NCHILDREN; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      void *mapped = map_shared_pages((void *)shared_va, parent_pid, PGSIZE);
      if (!mapped) {
        printf("Child %d mapping failed\n", i);
        exit(1);
      }
      write_log_messages((char*)mapped, i);
      exit(0);
    }
  }

  // Parent: read logs
  char *ptr = shared;
  char *end = ptr + PGSIZE;

  while (1) {
    ptr = (char *)(((uint64)ptr + 3) & ~3);
    if (ptr + 4 > end)
      break;

    uint32 raw = *(uint32*)ptr;

    if (raw == 0) {
      if ((end - ptr) < 5) {
      // Not enough space for a full message — done
      break;
      } else {
      // Incomplete header: probably not written yet
      continue;
      }
    }

    if (!is_header_complete(raw)) {
      printf("incomplete header at %p: raw=0x%x\n", ptr, raw);
      continue;
    }
    else
      printf("parent read complete header at %p\n", ptr);

    uint16 index = get_header_index(raw);
    uint16 length = get_header_length(raw);

    // Sanity check: length must be reasonable
    if (length == 0 || length > (end - ptr - 4)) {
      printf("!!! CORRUPTED HEADER at %p: raw=0x%x, index=%d, length=%d, remaining=%d\n",
       ptr, raw, index, length, (int)(end - ptr - 4));
      break;  // stop reading further — prevent segfaults
    }

    printf("[child %d] ", index);
    write(1, ptr + 4, length);
    printf("\n");

    ptr += 4 + length;
  }

  exit(0);
}
