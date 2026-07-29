#include <stdio.h>
#include <stdlib.h>

int main(void) {
  int *data = malloc(100 * sizeof(int));
  free(data);
  printf("%d\n", data[20]);
}

/*
Create a program that allocates an array of integers (as above), frees
them, and then tries to print the value of one of the elements of
the array. Does the program run? What happens when you use
valgrind on it?

R: The program does run, because we are close enough to it's memory bounds
even though reading unallocated memory, and printed value is 0. Valgring
is able to detect the invalid read and warns about it.
*/
