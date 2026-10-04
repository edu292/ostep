#include "mapreduce.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define DEFINE_QUEUE(T, name)                                                  \
  typedef struct name {                                                        \
    T *data;                                                                   \
    size_t head;                                                               \
    size_t tail;                                                               \
    size_t capacity;                                                           \
  } name;                                                                      \
                                                                               \
  static inline bool name##_init(name *q, size_t capacity) {                   \
    q->capacity = capacity > 0 ? capacity : 128;                               \
    q->head = 0;                                                               \
    q->tail = 0;                                                               \
    q->data = (T *)malloc(q->capacity * sizeof(T));                            \
    return q->data != nullptr;                                                 \
  }                                                                            \
                                                                               \
  static inline void name##_destroy(name *q) {                                 \
    free(q->data);                                                             \
    q->data = nullptr;                                                         \
    q->head = 0;                                                               \
    q->tail = 0;                                                               \
    q->capacity = 0;                                                           \
  }                                                                            \
                                                                               \
  static inline bool name##_grow(name *q) {                                    \
    size_t new_cap = q->capacity * 2;                                          \
    T *new_data = (T *)realloc(q->data, new_cap * sizeof(T));                  \
    if (!new_data) {                                                           \
      return false;                                                            \
    }                                                                          \
                                                                               \
    q->data = new_data;                                                        \
    q->capacity = new_cap;                                                     \
    return true;                                                               \
  }                                                                            \
                                                                               \
  static inline bool name##_push(name *q, T value) {                           \
    if (q->tail == q->capacity && !name##_grow(q)) {                           \
      return false;                                                            \
    }                                                                          \
                                                                               \
    q->data[q->tail++] = value;                                                \
    return true;                                                               \
  }                                                                            \
                                                                               \
  static inline bool name##_pop(name *q, T *out) {                             \
    if (q->head == q->tail) {                                                  \
      return false;                                                            \
    }                                                                          \
                                                                               \
    *out = q->data[q->head++];                                                 \
    return true;                                                               \
  }                                                                            \
                                                                               \
  static inline size_t name##_count(const name *q) {                           \
    return q->tail - q->head;                                                  \
  }                                                                            \
                                                                               \
  static inline T *name##_expand(name *q) {                                    \
    if (q->tail == q->capacity && !name##_grow(q))                             \
      return nullptr;                                                          \
    return &q->data[q->tail++];                                                \
  }

typedef struct {
  char *key;
  size_t index;
} KeyEntry;

DEFINE_QUEUE(KeyEntry, KeyEntryQueue)

static inline size_t KeyEntryQueue_search(const KeyEntryQueue *q,
                                          const char *key) {
  size_t low = q->head;
  size_t high = q->tail;

  while (low < high) {
    size_t mid = low + ((high - low) / 2);
    if (strcmp(q->data[mid].key, key) < 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  return low;
}

static inline bool KeyEntryQueue_sorted_insert(KeyEntryQueue *q,
                                               KeyEntry item) {
  if (q->tail == q->capacity && !KeyEntryQueue_grow(q)) {
    return false;
  }

  size_t pos = KeyEntryQueue_search(q, item.key);

  if (pos < q->tail) {
    memmove(&q->data[pos + 1], &q->data[pos],
            (q->tail - pos) * sizeof(KeyEntry));
  }

  q->data[pos] = item;
  q->tail++;
  return true;
}

static inline bool KeyEntryQueue_get_index(KeyEntryQueue *q, char *key,
                                           size_t *out_index) {
  size_t pos = KeyEntryQueue_search(q, key);

  if (pos == q->tail || strcmp(q->data[pos].key, key) != 0) {
    return false;
  }

  *out_index = q->data[pos].index;
  return true;
}

DEFINE_QUEUE(char *, StrQueue)

void StrQueue_init_from_data(StrQueue *q, char **data, size_t capacity) {
  q->head = 0;
  q->tail = capacity;
  q->capacity = capacity;
  q->data = data;
}

static inline bool StrQueue_pop_atomic(StrQueue *q, char **out) {
  size_t idx = atomic_fetch_add_explicit((_Atomic size_t *)&q->head, 1,
                                         memory_order_relaxed);

  if (idx < q->tail) {
    *out = q->data[idx];
    return true;
  }

  return false;
}

static inline void StrQueue_destroy_elements(StrQueue *q) {
  for (size_t i = 0; i < q->tail; i++) {
    free(q->data[i]);
  }

  StrQueue_destroy(q);
}

DEFINE_QUEUE(StrQueue, StrBuckets)

typedef struct {
  mtx_t lock;
  StrBuckets buckets;
  KeyEntryQueue key_entries;
} Partition;

typedef struct {
  Partitioner partitioner;
  size_t partition_count;
  Partition partitions[];
} IntermediateTable;

IntermediateTable *intermediate_table_create(Partitioner partitioner,
                                             size_t partition_count) {
  size_t total_size =
      sizeof(IntermediateTable) + (partition_count * sizeof(Partition));
  IntermediateTable *t = (IntermediateTable *)malloc(total_size);
  if (t == nullptr) {
    return nullptr;
  }

  t->partitioner = partitioner;
  t->partition_count = partition_count;

  for (size_t i = 0; i < partition_count; i++) {
    Partition *p = &t->partitions[i];
    mtx_init(&p->lock, mtx_plain);
    StrBuckets_init(&p->buckets, 16);
    KeyEntryQueue_init(&p->key_entries, 16);
  }

  return t;
}

void intermediate_table_free(IntermediateTable *t) {
  for (size_t i = 0; i < t->partition_count; i++) {
    Partition *p = t->partitions + i;

    for (size_t b = 0; b < p->buckets.tail; b++) {
      StrQueue_destroy_elements(p->buckets.data + b);
    }

    StrBuckets_destroy(&p->buckets);
    for (size_t k = p->key_entries.head; k < p->key_entries.tail; k++) {
      free(p->key_entries.data[k].key);
    }
    KeyEntryQueue_destroy(&p->key_entries);
    mtx_destroy(&p->lock);
  }

  free(t);
}

IntermediateTable *table;

void MR_Emit(char *key, char *value) {
  unsigned long p_idx = table->partitioner(key, (int)table->partition_count);
  Partition *p = table->partitions + p_idx;
  mtx_lock(&p->lock);

  size_t index;
  StrQueue *values;
  if (!KeyEntryQueue_get_index(&p->key_entries, key, &index)) {
    index = StrBuckets_count(&p->buckets);
    values = StrBuckets_expand(&p->buckets);
    StrQueue_init(values, 0);
    KeyEntryQueue_sorted_insert(&p->key_entries,
                                (KeyEntry){.key = strdup(key), .index = index});
  } else {
    values = &p->buckets.data[index];
  }

  StrQueue_push(values, strdup(value));
  mtx_unlock(&p->lock);
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0') {
    hash = (hash * 33) + c;
  }

  return hash % num_partitions;
}

char *get_next(char *key, int partition_number) {
  Partition *p = table->partitions + partition_number;

  size_t index;
  if (!KeyEntryQueue_get_index(&p->key_entries, key, &index)) {
    return nullptr;
  }

  StrQueue *values = p->buckets.data + index;
  char *value = nullptr;
  StrQueue_pop(values, &value);

  return value;
}

typedef struct {
  Mapper mapper;
  StrQueue filenames;
} MapperContext;

int mapper_worker(void *v) {
  MapperContext *ctx = (MapperContext *)v;
  char *filename;
  while (StrQueue_pop_atomic(&ctx->filenames, &filename)) {
    ctx->mapper(filename);
  }

  return EXIT_SUCCESS;
}

typedef struct {
  Reducer reducer;
  size_t partition_idx;
} ReducerContext;

int reducer_worker(void *v) {
  ReducerContext *ctx = (ReducerContext *)v;
  Partition *p = table->partitions + ctx->partition_idx;
  for (size_t i = 0; i < p->key_entries.tail; i++) {
    ctx->reducer(p->key_entries.data[i].key, get_next, (int)ctx->partition_idx);
  }
  return EXIT_SUCCESS;
}

void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce,
            int num_reducers, Partitioner partition) {
  table = intermediate_table_create(partition, (size_t)num_reducers);

  int file_count = argc - 1;

  int mapper_count = file_count >= num_mappers ? num_mappers : file_count;

  thrd_t mappers[mapper_count];
  MapperContext mapper_ctx = {.mapper = map};
  StrQueue_init_from_data(&mapper_ctx.filenames, argv + 1, (size_t)file_count);
  for (int t = 0; t < mapper_count; t++) {
    thrd_create(mappers + t, mapper_worker, &mapper_ctx);
  }

  for (int t = 0; t < mapper_count; t++) {
    thrd_join(mappers[t], nullptr);
  }

  thrd_t reducers[num_reducers];
  ReducerContext reducer_ctx[num_reducers];
  for (size_t t = 0; t < (size_t)num_reducers; t++) {
    ReducerContext *ctx = reducer_ctx + t;
    ctx->reducer = reduce;
    ctx->partition_idx = t;
    thrd_create(reducers + t, reducer_worker, ctx);
  }

  for (int t = 0; t < num_reducers; t++) {
    thrd_join(reducers[t], nullptr);
  }

  intermediate_table_free(table);
}
