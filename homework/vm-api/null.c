#include <stdio.h>

int main(void) {
  int *null = NULL;
  *null = 2;
}

/*
First, write a simple program called null.c that creates a pointer
to an integer, sets it to NULL, and then tries to dereference it. Com-
pile this into an executable called null. What happens when you
run this program?

R: It causes a segmentation fault. Address 0(null) is not accessible.
 */
