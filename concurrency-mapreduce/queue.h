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
  static inline size_t name##_count(const name *q) { return q->tail - q->head; }
