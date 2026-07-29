#include <stdlib.h>

int main(void) {
  char *buf = malloc(512);
  return 0;
}

/*
Write a simple program that allocates memory using malloc() but
forgets to free it before exiting. What happens when this program
runs? Can you use gdb to find any problems with it? How about
valgrind (again with the --leak-check=yes flag)?

R: Nothing visible happens because of forgetting to call free.
Gdb would allow me to examine the code in more detail in the assembly level
and that might help find the leak, but it has no mechanism to warn me about it.
Valgring on the other hand is made exactly for this and shows me stats
about the leak and where it is.
 */
