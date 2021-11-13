/*
 * humalloc.cpp
 *
 * Copyright (C) 2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <signal.h>
#include <unistd.h>

#if defined (__APPLE__)
#include <malloc/malloc.h>
#endif

#include <sys/mman.h>

#include "hulog.h"
#include "humain.h"


/* ----------- File Global Variables ----------------------------- */
/* Config */
static bool hu_malloc_inited = false;
static bool hu_overflow = false;
static bool hu_useafterfree = false;
static size_t hu_minsize = 0;
static size_t hu_size_multiple = 2 * sizeof(void*); // double word alignment

/* Runtime */
static long hu_page_size = 0;
static long hu_num_pages = 0;

struct hu_alloc_info
{
  void* user_ptr = nullptr;
  size_t user_size = 0;
  void* sys_ptr = nullptr;
  size_t sys_size = 0;
};

static std::unordered_set<void*>* hu_user_addrs = nullptr;
static std::unordered_set<void*>* hu_free_addrs = nullptr;
static std::unordered_map<void*, hu_alloc_info>* hu_active_allocs = nullptr;
static std::queue<hu_alloc_info>* hu_quarantine_allocs = nullptr;
static size_t hu_quarantine_size = 0;
static size_t hu_quarantine_max_size = 0;


/* ----------- Local Functions ----------------------------------- */
static inline size_t hu_round_up(size_t num_to_round, size_t multiple)
{
  if (multiple == 0) return num_to_round;

  int remainder = num_to_round % multiple;
  if (remainder == 0) return num_to_round;

  return (num_to_round + multiple - remainder);
}

static inline size_t hu_calc_user_size(size_t user_size)
{
  const size_t rounded_user_size = hu_round_up(user_size, hu_size_multiple);
  return rounded_user_size;
}

static inline size_t hu_calc_sys_size(size_t user_size)
{
  const size_t padded_size =
    hu_round_up(user_size, hu_page_size) + (hu_overflow ? hu_page_size : 0);
  return padded_size;
}

static inline bool hu_get_allocinfo(void* user_ptr, hu_alloc_info* alloc_info)
{
  auto it = hu_active_allocs->find(user_ptr);
  if (it == hu_active_allocs->end()) return false;

  *alloc_info = it->second;
  return true;
}

static int hu_mprotect(void* addr, size_t len, int prot)
{
#if defined(__linux__)
  static uint64_t callcount = 0;
  ++callcount;
#endif

  int rv = mprotect(addr, len, prot);
  if (rv != 0)
  {
    fprintf(stderr, "heapusage error: mprotect(%p, %ld, %d) failed errno %d\n",
            addr, len, prot, errno);

#if defined(__linux__)
    static const uint64_t max_map_count = []()
    {
      uint64_t val = 0;
      std::ifstream("/proc/sys/vm/max_map_count") >> val;
      return val;
    }();

    if (callcount > (max_map_count / 2))
    {
      fprintf(stderr,
              "max_map_count=%ld mprotect_count=%ld, try increasing max_map_count, ex::\n",
              max_map_count, callcount);
      fprintf(stderr, "sudo sh -c \"echo %ld > /proc/sys/vm/max_map_count\"\n",
              (2 * max_map_count));
      exit(1);
    }
#endif
  }

  return rv;
}


/* ----------- Global Functions ---------------------------------- */
void hu_malloc_init(bool overflow, bool useafterfree, size_t minsize)
{
  hu_overflow = overflow;
  hu_useafterfree = useafterfree;
  hu_minsize = minsize;
  
  hu_num_pages = sysconf(_SC_PHYS_PAGES);
  hu_page_size = sysconf(_SC_PAGE_SIZE);
  hu_quarantine_max_size = (((size_t)hu_num_pages * (size_t)hu_page_size) * 10 / 100);  

  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = hu_sig_handler;
#if defined(__linux__)
  sigaction(SIGSEGV, &sa, NULL);
#elif defined(__APPLE__)
  sigaction(SIGBUS, &sa, NULL);
#endif

  hu_user_addrs = new std::unordered_set<void*>();
  hu_active_allocs = new std::unordered_map<void*, hu_alloc_info>();
  hu_quarantine_allocs = new std::queue<hu_alloc_info>();
  hu_free_addrs = new std::unordered_set<void*>();
 
  hu_malloc_inited = true;
}

void hu_malloc_cleanup()
{
  hu_malloc_inited = false;
  return;
}

/*
 * humalloc implements a simple fenced malloc where each user allocation has
 * a read/write-protected page immedately after. Page size is system dependent, 
 * but commonly 4 KB.
 *
 *       <------------- N pages -------------> <--- 1 page --->
 *      | - - - - - - - - - ------------------|----------------|
 *      |   (pad to page)   | User Allocation | Protected Page |
 *      | - - - - - - - - - ------------------|----------------|
 *      ^                   ^
 *      |                   |
 *   sys_ptr             user_ptr
 *
 * Free'd user allocations are placed in a quarantine queue and fully
 * read/write protected from further access. The queue has a max size (currently
 * 10% of physical system RAM), and once full, the oldest allocations are made 
 * unprotected and free'd/returned back to the OS. At this point access to a 
 * free'd allocation cannot be detected anymore.
 *
 */
void* hu_malloc(size_t user_size)
{
  if (!hu_malloc_inited)
  {
    return malloc(user_size);
  }

  if (user_size == 0)
  {
    return malloc(user_size);
  }

  if (user_size < hu_minsize)
  {
    return malloc(user_size);
  }

  /* Calculate rounded user size */
  const size_t rounded_user_size = hu_calc_user_size(user_size);

  /* Calculate system memory needed */
  const size_t sys_size = hu_calc_sys_size(rounded_user_size);

  /* Allocate aligned at page size */
  void* sys_ptr = nullptr;
  int rv = posix_memalign(&sys_ptr, hu_page_size, sys_size);
  assert(sys_ptr != nullptr);
  assert(rv == 0);

  /* Add post fence protected page, if buffer overflow detection is enabled */
  void* post_fence_ptr = nullptr;
  if (hu_overflow)
  {
    post_fence_ptr = (char*)sys_ptr + sys_size - hu_page_size;
    hu_mprotect(post_fence_ptr, hu_page_size, PROT_NONE);
  }
  
  /* Calculate user pointer */
  void* user_ptr = nullptr;
  if (hu_overflow && (post_fence_ptr != nullptr))
  {
    user_ptr = (char*)post_fence_ptr - rounded_user_size;
  }
  else
  {
    user_ptr = sys_ptr;
  }

  /* Store allocation details */
  struct hu_alloc_info allocInfo;
  allocInfo.user_ptr = user_ptr;
  allocInfo.user_size = user_size;
  allocInfo.sys_ptr = sys_ptr;
  allocInfo.sys_size = sys_size;
  hu_active_allocs->insert(std::pair<void*, hu_alloc_info>(user_ptr, allocInfo));

  hu_user_addrs->insert(user_ptr);  
  hu_free_addrs->erase(user_ptr);

  return user_ptr ;
}

void hu_free(void* user_ptr)
{
  if (!hu_malloc_inited)
  {
    return free(user_ptr);
  }

  if (user_ptr == nullptr) return;

  if (!hu_user_addrs->count(user_ptr))
  {
    free(user_ptr);
    return;
  }
  
  /* Get allocation details */
  hu_alloc_info allocInfo;
  if (!hu_get_allocinfo(user_ptr, &allocInfo))
  {
    /* Double-free, ignored here and handled in log_event() */
    /* @todo: consider adding option to actually call free() here */
    return;
  }

  hu_active_allocs->erase(user_ptr);
  hu_free_addrs->insert(user_ptr);

  if (hu_useafterfree)
  {
    /* Quarantine allocation if use-after-free detection is enabled */
    hu_mprotect(allocInfo.sys_ptr, allocInfo.sys_size, PROT_NONE);
    
    hu_quarantine_allocs->push(allocInfo);
    hu_quarantine_size += allocInfo.sys_size;

    /* Release quarantined allocations back to OS if over max size */
    while ((hu_quarantine_size > hu_quarantine_max_size) && !hu_quarantine_allocs->empty())
    {
      hu_alloc_info delete_alloc_info = hu_quarantine_allocs->front();
      hu_quarantine_allocs->pop();

      hu_mprotect(delete_alloc_info.sys_ptr, delete_alloc_info.sys_size, PROT_READ | PROT_WRITE);
      hu_quarantine_size -= delete_alloc_info.sys_size;    
      free(delete_alloc_info.sys_ptr);
      hu_log_remove_freed_allocation(delete_alloc_info.user_ptr);
    }
  }
  else
  {
    /* Directly unprotect and release back to OS if use-after-free detection is disabled */
    hu_mprotect(allocInfo.sys_ptr, allocInfo.sys_size, PROT_READ | PROT_WRITE);
    free(allocInfo.sys_ptr);
  }
}

void* hu_calloc(size_t count, size_t size)
{
  if (!hu_malloc_inited)
  {
    return calloc(count, size);
  }

  if ((count == 0) || (size == 0))
  {
    return calloc(count, size);
  }

  void* ptr = hu_malloc(count * size);
  if (ptr != nullptr)
  {
    memset(ptr, 0, count * size);
  }
  
  return ptr;
}

void* hu_realloc(void *user_ptr, size_t user_size)
{
  if (!hu_malloc_inited)
  {
    return realloc(user_ptr, user_size);
  }

  if (user_ptr == nullptr)
  {
    void* new_user_ptr = hu_malloc(user_size);
    return new_user_ptr;
  }  
    
  if (user_size == 0)
  {
    hu_free(user_ptr);
    return nullptr;
  }

  hu_alloc_info allocInfo;
  if (hu_get_allocinfo(user_ptr, &allocInfo))
  {
    void* new_user_ptr = hu_malloc(user_size);
    if (new_user_ptr != nullptr)
    {
      size_t copy_size = std::min(user_size, allocInfo.user_size);
      memcpy(new_user_ptr, allocInfo.user_ptr, copy_size);
    }

    hu_free(user_ptr);

    return new_user_ptr;
  }
  else
  {
    return realloc(user_ptr, user_size);
  }
}

size_t hu_malloc_size(void* user_ptr)
{
  hu_alloc_info allocInfo;
  if (hu_malloc_inited && hu_get_allocinfo(user_ptr, &allocInfo))
  {
    const size_t rounded_user_size = hu_calc_user_size(allocInfo.user_size);
    return rounded_user_size;
  }
  else
  {
#if defined (__APPLE__)
    return malloc_size(user_ptr);
#else
    return 0;
#endif
  }
}
