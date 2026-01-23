#include "array.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void array_create(Array *vec, size_t element_size) {
	vec->element_size = element_size;
	vec->size = 0;
	vec->capacity = ARRAY_DEFAULT_SIZE * element_size;

	vec->ptr = malloc(vec->capacity);
	if (vec->ptr == NULL)
		fprintf(stderr, "array_create: malloc failed\n");
}

void array_destroy(Array *vec) {
	free(vec->ptr);
	vec->ptr = NULL;
}

void *array_add_empty(Array *vec) {
	if (vec->ptr == NULL) {
		fprintf(stderr, "array_add_empty detected null pointer\n");
		return NULL;
	}

	if ((vec->size + 1) * vec->element_size > vec->capacity) {
		vec->capacity *= 2;

		vec->ptr = realloc(vec->ptr, vec->capacity);
	}

	return ((uint8_t *)vec->ptr + (vec->size++) * vec->element_size);
}

void array_clear(Array *vec) {
	if (vec->ptr == NULL) {
		fprintf(stderr, "array_clear detected null pointer");
		return;
	}

	vec->size = 0;

	if (vec->capacity > 8192) {
		vec->capacity = ARRAY_DEFAULT_SIZE * vec->element_size;
		vec->ptr = realloc(vec->ptr, vec->capacity);
		if (vec->ptr == NULL)
			fprintf(stderr, "array_clear: realloc failed\n");
	}
}
