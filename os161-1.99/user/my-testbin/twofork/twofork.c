#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

int
main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  pid_t pid;
  pid_t pid2;
  pid = fork();
  pid2 = fork();
  if (pid < 0) {
    warn("fork");
  }
  else if (pid == 0) {
    /* child */
    putchar('C');
    putchar('\n');
  }
  else {
    /* parent */
    putchar('P');
    putchar('\n');
  }

    if (pid2 < 0) {
    warn("fork");
  }
  else if (pid2 == 0) {
    /* child */
    putchar('C2');
    putchar('\n');
  }
  else {
    /* parent */
    putchar('P2');
    putchar('\n');
  }
  return(0);
}