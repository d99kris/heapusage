/*
 * humain.c
 *
 * Copyright (C) 2017 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <stdlib.h>

#include "hulog.h"
#include "humain.h"


/* ----------- Global Functions ---------------------------------- */
void __attribute__ ((constructor)) nu_init(void)
{
  /* Init logging */
  log_init();
    
  /* Enable logging */
  log_enable(1);
}

void __attribute__ ((destructor)) nu_fini(void)
{
  /* Disable logging */
  log_enable(0);

  /* Present result */
  log_summary();
}


#ifdef __linux__
/* ----------- Linux Wrapper Functions --------------------------- */
/* Extern declarations of glibc actual heap functions */
extern void *__libc_malloc(size_t size);
extern void __libc_free(void *ptr);
extern void *__libc_calloc(size_t nmemb, size_t size);
extern void *__libc_realloc(void *ptr, size_t size);

void *malloc(size_t size)
{
  void *ptr = __libc_malloc(size);
  log_event(EVENT_MALLOC, ptr, size);
  return ptr;
}

void free(void* ptr)
{
  log_event(EVENT_FREE, ptr, 0);
  __libc_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
  void *ptr = __libc_calloc(nmemb, size);
  log_event(EVENT_MALLOC, ptr, nmemb * size);
  return ptr;
}

void *realloc(void *ptr, size_t size)
{
  void *newptr = __libc_realloc(ptr, size);
  log_event(EVENT_FREE, ptr, 0);
  log_event(EVENT_MALLOC, newptr, size);
  return newptr;
}


#elif __APPLE__
/* ----------- Apple Wrapper Functions --------------------------- */
#define DYLD_INTERPOSE(_newfun, _orgfun) \
__attribute__((used)) static struct{ const void *newfun; const void *orgfun; } _interpose_##_orgfun \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void *)(unsigned long)&_newfun, \
                                                     (const void *)(unsigned long)&_orgfun }

void *malloc_wrap(size_t size);
void free_wrap(void* ptr);
void* calloc_wrap(size_t count, size_t size);
void* realloc_wrap(void *ptr, size_t size);
void* valloc_wrap(size_t size);

void *malloc_wrap(size_t size)
{
  void *ptr = malloc(size);
  log_event(EVENT_MALLOC, ptr, size);
  return ptr;
}
DYLD_INTERPOSE(malloc_wrap, malloc);

void free_wrap(void* ptr)
{
  log_event(EVENT_FREE, ptr, 0);
  free(ptr);
}
DYLD_INTERPOSE(free_wrap, free);

void* calloc_wrap(size_t count, size_t size)
{
  void *ptr = calloc(count, size);
  log_event(EVENT_MALLOC, ptr, count * size);
  return ptr;
}
DYLD_INTERPOSE(calloc_wrap, calloc);

void* realloc_wrap(void *ptr, size_t size)
{
  void *newptr = realloc(ptr, size);
  log_event(EVENT_FREE, ptr, 0);
  log_event(EVENT_MALLOC, newptr, size);
  return newptr;
}
DYLD_INTERPOSE(realloc_wrap, realloc);

void* valloc_wrap(size_t size)
{
  void *ptr = valloc(size);
  log_event(EVENT_MALLOC, ptr, size);
  return ptr;
}
DYLD_INTERPOSE(valloc_wrap, valloc);


#else
#warning "Unsupported platform"
#endif

