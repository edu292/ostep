#include <stdlib.h>

int main(void) {
    int *data = malloc(sizeof(int) * 100);
    data[100] = 67;
    free(data);
}

/*
Write a program that creates an array of integers called data of size
100 using malloc; then, set data[100] to zero. What happens
when you run this program? What happens when you run this
program using valgrind? Is the program correct?

R: The program is incorrect because arrays are 0 indexed, thus for length
100 the index 100 is out of bounds, but since it is still close to the program
memory bounds it may not cause a segfault. When running with valgrind it is
able to detect the invalid write and generate a warning.
 */
