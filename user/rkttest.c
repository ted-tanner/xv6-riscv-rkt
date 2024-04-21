#include "kernel/rkt.h"
#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096

void thread_main(void *arg) {
  sleep(10);
  printf("Hello from thread!\n");

  int result = exec("sh", 0);
  printf("Thread exec result: %d\n", result);
  
  exit(0);
}

int main(int argc, char *argv[]) {
  void *stack = malloc(PGSIZE);
  int thread_id = clone(&thread_main, 0, stack, RKT_RESTRICT_EXEC | RKT_RESTRICT_KILL);

  printf("Hello from main!\n");
  printf("Thread ID: %d\n", thread_id);

  sleep(25);

  return 0;
}

// TODO: Test can both access static memory
// TODO: Test can both access heap memory
