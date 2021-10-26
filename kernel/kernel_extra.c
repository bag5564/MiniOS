/*
 * kernel_extra.c - Project 2 Extra Credit, CMPSC 473
 * Copyright 2021 Ruslan Nikolaev <rnikola@psu.edu>
 * Distribution, modification, or usage without explicit author's permission
 * is not allowed.
 */

#include <malloc.h>
#include <types.h>
#include <string.h>
#include <printf.h>

// Your mm_init(), malloc(), free() code from mm.c here
// You can only use mem_sbrk(), mem_heap_lo(), mem_heap_hi() and
// Project 2's kernel headers provided in 'include' such
// as memset and memcpy.
// No other files from Project 1 are allowed!

bool mm_init()
{
	return false;
}

void *malloc(size_t size)
{
	return NULL;
}

void free(void *ptr)
{
}
