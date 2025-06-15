#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/msg_protocol.h"
#include "user/msg_protocol.c"

int
main(int argc, char *argv[]) {
  int parent_pid = getpid();
  void *shared = malloc(PGSIZE);
  if (!shared) {
    printf("malloc failed\n");
    exit(1);
  }

  memset(shared, 0, PGSIZE);
  uint64 shared_va = (uint64)shared;

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // child: write to shared buffer
    void *mapped = map_shared_pages((void *)shared_va, parent_pid, PGSIZE);
    if (!mapped) {
      printf("Child mapping failed\n");
      exit(1);
    }
    write_log_messages((char*)mapped, 7);
    printf("child finished writing to buffer\n");
    exit(0);
  }

  // parent: concurrently read what's written
  char *ptr = shared;
  char *end = ptr + PGSIZE;

  while (1) {
    ptr = (char *)(((uint64)ptr + 3) & ~3);
    if (ptr + 4 > end) break;

    uint32 raw = *(uint32*)ptr;
    if (raw == 0) {
      // nothing written yet — small sleep and try again
      sleep(1);
      continue;
    }

    uint16 index = raw >> 16;
    uint16 length = raw & 0xFFFF;

    printf("[child %d] ", index);
    write(1, ptr + 4, length);
    printf("\n");

    ptr += (4 + length);
  }

  exit(0);
}