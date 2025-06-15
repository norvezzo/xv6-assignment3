#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SHARED_SIZE 4096

int
main(int argc, char *argv[])
{
  int disable_unmap = 0;
  if (argc > 1 && strcmp(argv[1], "keep") == 0)
    disable_unmap = 1;

  int parent_pid = getpid();
  void *shared = malloc(SHARED_SIZE);
  if (!shared) {
    printf("Parent malloc failed\n");
    exit(1);
  }

  uint64 shared_va = (uint64)shared;

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // --- Child ---
    printf("Child size before mapping: %d\n", sbrk(0));

    void *mapped = map_shared_pages((void *)shared_va, parent_pid, SHARED_SIZE);
    if (!mapped) {
      printf("Child mapping failed\n");
      exit(1);
    }

    strcpy((char *)mapped, "Hello daddy");
    printf("Child size after mapping: %d\n", sbrk(0));

    if (!disable_unmap) {
      if (unmap_shared_pages(mapped, SHARED_SIZE) == 0)
        printf("Child size after unmapping: %d\n", sbrk(0));
    }

    void *m = malloc(256);
    if (m)
      printf("Child size after malloc: %d\n", sbrk(0));
    else
      printf("Child malloc failed\n");

    exit(0);
  } 
  else {
    // --- Parent ---
    wait(0);
    printf("Parent reads: %s\n", (char *)shared);

    exit(0);
  }
}