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

  int p[2]; // pipe to send mapped VA to child
  pipe(p);

  int pid = fork();

  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    close(p[1]);
    // read mapped VA from parent
    uint64 child_va;
    if (read(p[0], &child_va, sizeof(child_va)) != sizeof(child_va)) {
      printf("Child failed to read address\n");
      exit(1);
    }
    close(p[0]);

    printf("Child size before mapping: %d\n", sbrk(0));

    // write to the shared memory using received address
    char *shared = (char *)child_va;
    strcpy(shared, "Hello daddy");

    printf("Child size after mapping: %d\n", sbrk(0));

    if (!disable_unmap) {
      if (unmap_shared_pages(shared, SHARED_SIZE) == 0)
        printf("Child size after unmapping: %d\n", sbrk(0));
    }

    void *new_mem = malloc(256);
    if (new_mem)
      printf("Child size after malloc: %d\n", sbrk(0));
    else
      printf("Child malloc failed\n");
    
    exit(0);

  } else {
    close(p[0]);
    // allocate memory after fork so it's only in the parent
    char *shared = malloc(SHARED_SIZE);
    if (!shared) {
      printf("Parent malloc failed\n");
      exit(1);
    }

    // map to child process
    uint64 mapped_va = (uint64) map_shared_pages(shared, pid, SHARED_SIZE);
    if (mapped_va == 0) {
      printf("Parent map_shared_pages failed\n");
      exit(1);
    }

    // send mapped VA to child
    write(p[1], &mapped_va, sizeof(mapped_va));
    close(p[1]);

    wait(0); // wait for child

    printf("Parent reads: %s\n", shared);

    exit(0);
  }
}