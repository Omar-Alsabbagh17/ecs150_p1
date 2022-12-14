#ifndef STRING_VEC_H
#define STRING_VEC_H

# define INIT_CAP 100

typedef struct {
    void **items;
    int capacity;
    int total;
} string_vector;


void vec_init(string_vector *);
void vec_add(string_vector *, void *);
char* vec_pop(string_vector * );

#endif
