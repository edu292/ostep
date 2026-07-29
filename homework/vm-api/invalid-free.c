#include <stdlib.h>
int main(void) {
  int *data = malloc(128 * sizeof(int));
  free(&data[25]);
}

/*
Now pass a funny value to free (e.g., a pointer in the middle of the
array you allocated above). What happens? Do you need tools to
find this type of problem?

R: The compiler does not find it so funny and warns about offsetting (indexing)
a value before passing to free. On runtime this causes a segfault. Valgring also
shows more information about the invalid free and the resulting segfault and leaked
memory.
 */
