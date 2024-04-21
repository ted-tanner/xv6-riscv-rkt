#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096

void thread_main(void *arg) {
  // sleep(10);
  printf("Hello from thread!\n");
  exit(0);
}

int main(int argc, char *argv[]) {
  printf("thread_main: %p, main: %p\n", &thread_main, &main);

  void *stack = malloc(PGSIZE);
  int thread_id = clone(&thread_main, 0, stack, 0);

  printf("Hello from main!\n");
  // printf("Thread ID: %d\n", thread_id);

  sleep(20);

  return 0;
}

// TODO: Test can both access static memory
// TODO: Test can both access heap memory
