#pragma once

#define ARRAY_DEFAULT_SIZE 16

#include <stddef.h>
#include <stdbool.h>

typedef struct {
	size_t element_size;
	size_t size;
	size_t capacity;
	void *ptr;
} Array;

void array_create(Array *vec, size_t element_size);
void array_destroy(Array *vec);

void array_add(Array *vec, void *ptr);
void *array_add_empty(Array *vec); // something like emplace
void array_clear(Array *vec);
