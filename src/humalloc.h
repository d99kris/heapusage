/*
 * humalloc.h
 *
 * Copyright (C) 2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Global Function Prototypes ------------------------ */
void hu_malloc_init(bool overflow, bool useafterfree, size_t minsize);
void hu_malloc_cleanup();

void* hu_malloc(size_t user_size);
void hu_free(void* ptr);
void* hu_calloc(size_t count, size_t size);
void* hu_realloc(void *ptr, size_t size);
