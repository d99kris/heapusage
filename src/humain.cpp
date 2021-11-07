/*
 * humain.cpp
 *
 * Copyright (C) 2017-2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <atomic>
#include <cstdio>
#include <cstring>

#include <limits.h>
#include <pthread.h>
#include <stdlib.h>

#include "hulog.h"
#include "humain.h"
#include "humalloc.h"


/* ----------- File Global Variables ----------------------------- */
/* Config */
static bool hu_doublefree = false;
static bool hu_leak = false;
static bool hu_overflow = false;
static bool hu_useafterfree = false;

static char hu_file[PATH_MAX];
static size_t hu_minsize = 0;
static bool hu_nosyms = 0;

/* State */
static bool hu_enable_humalloc = false;
static bool hu_bypass = false;

/* Recursion detection */
static int hu_callcount = 0;
#ifdef __APPLE__
static pthread_mutex_t hu_recursive_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif __linux__
static pthread_mutex_t hu_recursive_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
#warning "unsupported platform"
#endif


/* ----------- Local Functions ----------------------------------- */
class hu_recursion_checker
{
public:
  hu_recursion_checker()
  {
    pthread_mutex_lock(&hu_recursive_lock);
    ++hu_callcount;
  }

  ~hu_recursion_checker()
  {
    --hu_callcount;
    pthread_mutex_unlock(&hu_recursive_lock);
  }

  bool is_recursive_call()
  {
    return (hu_callcount > 1);
  }
};

static inline bool hu_get_env_bool(const char* name)
{
  char* value = getenv(name);
  if (value == nullptr) return false;

  return (strcmp(value, "1") == 0);
}


/* ----------- Global Functions ---------------------------------- */
extern "C"
void __attribute__ ((constructor)) hu_init(void)
{
  /* Read config from env */
  hu_doublefree = hu_get_env_bool("HU_DOUBLEFREE");
  hu_leak = hu_get_env_bool("HU_LEAK");
  hu_overflow = hu_get_env_bool("HU_OVERFLOW");
  hu_useafterfree = hu_get_env_bool("HU_USEAFTERFREE");
  
  realpath(getenv("HU_FILE"), hu_file);
  hu_minsize = getenv("HU_MINSIZE") ? strtoll(getenv("HU_MINSIZE"), NULL, 10) : 0;
  hu_nosyms = hu_get_env_bool("HU_NOSYMS");

  /* Init logging */
  log_init(hu_file, hu_doublefree, hu_nosyms, hu_minsize, hu_useafterfree, hu_leak);
  
  /* Init custom malloc */
  hu_enable_humalloc = (hu_overflow || hu_useafterfree);
  if (hu_enable_humalloc)
  {
    hu_malloc_init(hu_overflow, hu_useafterfree, hu_minsize);
  }

  /* Do not enable preload for child processes */
  unsetenv("DYLD_INSERT_LIBRARIES");
  unsetenv("LD_PRELOAD");

  /* Enable logging */
  log_enable(1);
}

extern "C"
void __attribute__ ((destructor)) hu_fini(void)
{
  /* Disable logging */
  log_enable(0);

  /* Cleanup malloc */
  if (hu_enable_humalloc)
  {
    hu_enable_humalloc = false;
    hu_malloc_cleanup();
  }

  /* Present result */
  log_summary();
}

void hu_set_bypass(bool bypass)
{
  hu_bypass = bypass;
}


#if defined(__linux__)
/* ----------- Linux Wrapper Functions --------------------------- */
/* Extern declarations of glibc actual heap functions */
extern "C" void *__libc_malloc(size_t size);
extern "C" void __libc_free(void *ptr);
extern "C" void *__libc_calloc(size_t nmemb, size_t size);
extern "C" void *__libc_realloc(void *ptr, size_t size);

extern "C"
void *malloc(size_t size)
{
  if (hu_bypass) return __libc_malloc(size);
  
  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return __libc_malloc(size);
  
  void *ptr = hu_enable_humalloc ? hu_malloc(size) : __libc_malloc(size);
  log_event(EVENT_MALLOC, ptr, size);

  return ptr;
}

extern "C"
void free(void* ptr)
{
  if (hu_bypass) return __libc_free(ptr);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return __libc_free(ptr);

  hu_enable_humalloc ? hu_free(ptr) : __libc_free(ptr);
  log_event(EVENT_FREE, ptr, 0);
}

extern "C"
void *calloc(size_t nmemb, size_t size)
{
  if (hu_bypass) return __libc_calloc(nmemb, size);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return __libc_calloc(nmemb, size);

  void *ptr = hu_enable_humalloc ? hu_calloc(nmemb, size) : __libc_calloc(nmemb, size);;
  log_event(EVENT_MALLOC, ptr, nmemb * size);

  return ptr;
}

extern "C"
void *realloc(void *ptr, size_t size)
{ 
  if (hu_bypass) return __libc_realloc(ptr, size);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return __libc_realloc(ptr, size);

  void *newptr = hu_enable_humalloc ? hu_realloc(ptr, size) : __libc_realloc(ptr, size);
  if (ptr != nullptr)
  {
    log_event(EVENT_FREE, ptr, 0);
  }

  if (size != 0)
  {
    log_event(EVENT_MALLOC, newptr, size);
  }

  return newptr;
}


#elif defined(__APPLE__)
/* ----------- Apple Wrapper Functions --------------------------- */
#define DYLD_INTERPOSE(_newfun, _orgfun) \
__attribute__((used)) static struct{ const void *newfun; const void *orgfun; } _interpose_##_orgfun \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void *)(unsigned long)&_newfun, \
                                                     (const void *)(unsigned long)&_orgfun }

extern "C" void *malloc_wrap(size_t size);
extern "C" void free_wrap(void* ptr);
extern "C" void* calloc_wrap(size_t count, size_t size);
extern "C" void* realloc_wrap(void *ptr, size_t size);

extern "C"
void *malloc_wrap(size_t size)
{
  if (hu_bypass) return malloc(size);
  
  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return malloc(size);
  
  void *ptr = hu_enable_humalloc ? hu_malloc(size) : malloc(size);
  log_event(EVENT_MALLOC, ptr, size);

  return ptr;
}
DYLD_INTERPOSE(malloc_wrap, malloc);

extern "C"
void free_wrap(void* ptr)
{
  if (hu_bypass) return free(ptr);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return free(ptr);

  hu_enable_humalloc ? hu_free(ptr) : free(ptr);
  log_event(EVENT_FREE, ptr, 0);
}
DYLD_INTERPOSE(free_wrap, free);

extern "C"
void* calloc_wrap(size_t nmemb, size_t size)
{
  if (hu_bypass) return calloc(nmemb, size);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return calloc(nmemb, size);

  void *ptr = hu_enable_humalloc ? hu_calloc(nmemb, size) : calloc(nmemb, size);;
  log_event(EVENT_MALLOC, ptr, nmemb * size);

  return ptr;
}
DYLD_INTERPOSE(calloc_wrap, calloc);

extern "C"
void* realloc_wrap(void *ptr, size_t size)
{
  if (hu_bypass) return realloc(ptr, size);

  hu_recursion_checker recursion_checker;
  if (recursion_checker.is_recursive_call()) return realloc(ptr, size);

  void *newptr = hu_enable_humalloc ? hu_realloc(ptr, size) : realloc(ptr, size);
  if (ptr != nullptr)
  {
    log_event(EVENT_FREE, ptr, 0);
  }

  if (size != 0)
  {
    log_event(EVENT_MALLOC, newptr, size);
  }

  return newptr;
}
DYLD_INTERPOSE(realloc_wrap, realloc);


#else
#warning "Unsupported platform"
#endif

