#include <stdbool.h>
#include <stdlib.h>

#define DEFAULT_CAPACITY 128

typedef struct {
  int *data;
  size_t length;
  size_t capacity;
} Vector;

typedef struct Node {
  int value;
  struct Node *next;
} Node;

typedef struct {
  Node *head;
  Node *tail;
  size_t length;
} List;

bool vector_init(Vector *v, size_t initial_capacity) {
  if (v == NULL) {
    return false;
  }

  size_t cap = initial_capacity > 0 ? initial_capacity : DEFAULT_CAPACITY;
  v->data = malloc(sizeof(int) * cap);
  if (v->data == NULL) {
    return false;
  }

  v->length = 0;
  v->capacity = cap;

  return true;
}

void vector_destroy(Vector *v) {
  if (v != NULL) {
    free(v->data);
    v->data = NULL;
    v->capacity = 0;
    v->length = 0;
  }
}

bool vector_insert(Vector *v, int val) {
  if (v == NULL) {
    return false;
  }

  if (v->length == v->capacity) {
    size_t new_cap = v->capacity * 2;
    int *new_data = realloc(v->data, new_cap * sizeof(int));
    if (new_data == NULL) {
      return false;
    }

    v->data = new_data;
    v->capacity = new_cap;
  }

  v->data[v->length++] = val;

  return true;
}

bool vector_get(Vector *v, size_t index, int *out) {
  if (v == NULL || v->data == NULL || index >= v->length) {
    return false;
  }

  *out = v->data[index];
  return true;
}

void list_init(List *l) {
  if (l == NULL) {
    return;
  }

  l->head = NULL;
  l->tail = NULL;
  l->length = 0;
}

void list_destroy(List *l) {
  if (l == NULL)
    return;

  Node *curr = l->head;
  while (curr != NULL) {
    Node *next = curr->next;
    free(curr);
    curr = next;
  }

  l->head = NULL;
  l->tail = NULL;
  l->length = 0;
}

bool list_insert(List *l, int val) {
  if (l == NULL) {
    return false;
  }

  Node *node = malloc(sizeof(Node));
  if (node == NULL) {
    return false;
  }

  node->value = val;
  node->next = NULL;
  if (l->tail == NULL) {
    l->tail = node;
    l->head = node;
  } else {
    l->tail->next = node;
    l->tail = node;
  }

  l->length++;
  return true;
}

bool list_get(List *l, size_t index, int *out) {
  if (l == NULL || index >= l->length || out == NULL) {
    return false;
  }

  Node *node = l->head;
  for (; index; index--) {
    node = node->next;
  }

  *out = node->value;
  return true;
}

/*
Try out some of the other interfaces to memory allocation. For ex-
ample, create a simple vector-like data structure and related rou-
tines that use realloc() to manage the vector. Use an array to
store the vectors elements; when a user adds an entry to the vec-
tor, use realloc() to allocate more space for it. How well does
such a vector perform? How does it compare to a linked list? Use
valgrind to help you find bugs.

R: A vector that calls realloc for every single insert would perform
worse than a linked list on insertion, while having faster reads. In
general it would be preferable to start the vector with some pre-allocated
space and when needed reallocate it to have some multiple of it's current
size. In this case it has a much smaller insert cost baseline, but with
ocasional bumps when it reaches it's size limit.
*/
