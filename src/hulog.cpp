/*
 * hulog.cpp
 *
 * Copyright (C) 2017 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "hulog.h"


/* ----------- Defines ------------------------------------------- */
#define MAX_CALL_STACK 20   /* Limits the callstack depth to store */


/* ----------- Types --------------------------------------------- */
typedef struct hu_allocinfo_s
{
  void *ptr;
  ssize_t size;
  void *callstack[MAX_CALL_STACK];
  int callstack_depth;
  int count;
} hu_allocinfo_t;


/* ----------- File Global Variables ----------------------------- */
static pid_t pid = 0;
static char *hu_log_file = NULL;
static int hu_log_nosyms = 0;
static ssize_t hu_log_minleak = 0;

static int callcount = 0;
static int logging_enabled = 0;
static pthread_mutex_t callcount_lock = PTHREAD_MUTEX_INITIALIZER;
#ifdef __APPLE__
static pthread_mutex_t recursive_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#elif __linux__
static pthread_mutex_t recursive_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
#warning "Unsupported platform"
#endif

static unsigned long long allocinfo_total_frees = 0;
static unsigned long long allocinfo_total_allocs = 0;
static unsigned long long allocinfo_total_alloc_bytes = 0;
static unsigned long long allocinfo_current_alloc_bytes = 0;
static unsigned long long allocinfo_peak_alloc_bytes = 0;

static std::map<void*, hu_allocinfo_t> allocations;
static std::map<void*, std::string> symbol_cache;


/* ----------- Local Functions ----------------------------------- */
struct size_compare
{
  bool operator() (const hu_allocinfo_t& lhs, const hu_allocinfo_t& rhs) const
  {
    return lhs.size < rhs.size;
  }
};

static std::string addr_to_symbol(void *addr);


/* ----------- Global Functions ---------------------------------- */
void log_init()
{
  /* Get runtime info */
  hu_log_file = getenv("HU_FILE");
  hu_log_nosyms = ((getenv("HU_NOSYMS") != NULL) && (strcmp(getenv("HU_NOSYMS"), "1") == 0));
  hu_log_minleak = getenv("HU_MINLEAK") ? strtoll(getenv("HU_MINLEAK"), NULL, 10) : 0;
  pid = getpid();

  /* Initial log output */
  if (hu_log_file)
  {
    FILE *f = fopen(hu_log_file, "w");
    if (f)
    {
      fprintf(f, "==%d== Heapusage - https://github.com/d99kris/heapusage\n", pid);
      fprintf(f, "==%d== \n", pid);
      fclose(f);
    }
    else
    {
      fprintf(stderr, "heapusage error: unable to open output file (%s) for writing\n", hu_log_file);
    }
  }
  else
  {
    fprintf(stderr, "heapusage error: no output file specified\n");
  }
}

void log_enable(int flag)
{
  logging_enabled = flag;
}

void log_print_callstack(FILE *f, int callstack_depth, void * const callstack[])
{
  if (callstack_depth > 0)
  {
    int i = 1;
    while (i < callstack_depth)
    {
#if UINTPTR_MAX == 0xffffffff
      fprintf(f, "==%d==    at 0x%08x", pid, (unsigned int) callstack[i]);
#else
      fprintf(f, "==%d==    at 0x%016" PRIxPTR, pid, (unsigned long) callstack[i]);
#endif

      if (hu_log_nosyms)
      {
        fprintf(f, "\n");
      }
      else
      {
        std::string symbol = addr_to_symbol(callstack[i]);
        fprintf(f, ": %s\n", symbol.c_str());
      }

      ++i;
    }
  }
  else
  {
    fprintf(f, "==%d==    error: backtrace() returned empty callstack\n", pid);
  }
}

void log_event(int event, void *ptr, size_t size)
{
  if (logging_enabled)
  {
    int in_recursion = 0;
    pthread_mutex_lock(&recursive_lock);
    pthread_mutex_lock(&callcount_lock);
    if (callcount == 0)
    {
      ++callcount;
    }
    else
    {
      in_recursion = 1;
    }
    pthread_mutex_unlock(&callcount_lock);

    if (!in_recursion)
    {
      if (event == EVENT_MALLOC)
      {
        hu_allocinfo_t allocinfo;
        allocinfo.size = size;
        allocinfo.ptr = ptr;
        allocinfo.callstack_depth = backtrace(allocinfo.callstack, MAX_CALL_STACK);
        allocinfo.count = 1;
        allocations[ptr] = allocinfo;
        
        allocinfo_total_allocs += 1;
        allocinfo_total_alloc_bytes += size;
        allocinfo_current_alloc_bytes += size;

        if (allocinfo_current_alloc_bytes > allocinfo_peak_alloc_bytes)
        {
          allocinfo_peak_alloc_bytes = allocinfo_current_alloc_bytes;
        }
      }
      else if (event == EVENT_FREE)
      {
        std::map<void*, hu_allocinfo_t>::iterator allocation = allocations.find(ptr);
        if (allocation != allocations.end())
        {
          allocinfo_current_alloc_bytes -= allocation->second.size;
        }
        else
        {
          FILE *f = fopen(hu_log_file, "a");
          if (f)
          {
            fprintf(f, "==%d== Invalid deallocation at:\n", pid);

            void *callstack[MAX_CALL_STACK];
            int callstack_depth = backtrace(callstack, MAX_CALL_STACK);
            log_print_callstack(f, callstack_depth, callstack);

            fprintf(f, "==%d== \n", pid);
            fclose(f);
          }
        }

        allocinfo_total_frees += 1;
        allocations.erase(ptr);
      }

      pthread_mutex_lock(&callcount_lock);
      --callcount;
      pthread_mutex_unlock(&callcount_lock);
    }

    pthread_mutex_unlock(&recursive_lock);
  }
}

void log_summary()
{
  FILE *f = NULL;
  if (hu_log_file)
  {
    f = fopen(hu_log_file, "a");
  }

  if (!f)
  {
    return;
  }

  unsigned long long leak_total_bytes = 0;
  unsigned long long leak_total_blocks = 0;

  /* Group results by callstack */
  static std::map<std::vector<void*>, hu_allocinfo_t> allocations_by_callstack;
  for (auto it = allocations.begin(); it != allocations.end(); ++it)
  {
    std::vector<void*> callstack;
    callstack.assign(it->second.callstack, it->second.callstack + it->second.callstack_depth);

    auto callstack_it = allocations_by_callstack.find(callstack);
    if (callstack_it != allocations_by_callstack.end())
    {
      callstack_it->second.count += 1;
      callstack_it->second.size += it->second.size;
    }
    else
    {
      allocations_by_callstack[callstack] = it->second;
    }

    leak_total_bytes += it->second.size;
    leak_total_blocks += 1;
  }

  /* Sort results by total allocation size */
  std::multiset<hu_allocinfo_t, size_compare> allocations_by_size;
  for (auto it = allocations_by_callstack.begin(); it != allocations_by_callstack.end(); ++it)
  {
      allocations_by_size.insert(it->second);
  }

  /* Output heap summary */
  fprintf(f, "==%d== HEAP SUMMARY:\n", pid);
  fprintf(f, "==%d==     in use at exit: %llu bytes in %llu blocks\n",
          pid, leak_total_bytes, leak_total_blocks);
  fprintf(f, "==%d==   total heap usage: %llu allocs, %llu frees, %llu bytes allocated\n",
          pid, allocinfo_total_allocs, allocinfo_total_frees, allocinfo_total_alloc_bytes);
  fprintf(f, "==%d==    peak heap usage: %llu bytes allocated\n",
          pid, allocinfo_peak_alloc_bytes);
  fprintf(f, "==%d== \n", pid);

  /* Output leak details */
  for (auto it = allocations_by_size.rbegin(); (it != allocations_by_size.rend()) && (it->size >= hu_log_minleak); ++it)
  {
    fprintf(f, "==%d== %zu bytes in %d block(s) are lost, originally allocated at:\n", pid, it->size, it->count);

    log_print_callstack(f, it->callstack_depth, it->callstack);
    
    fprintf(f, "==%d== \n", pid);
  }
  
  /* Output leak summary */
  fprintf(f, "==%d== LEAK SUMMARY:\n", pid);
  fprintf(f, "==%d==    definitely lost: %llu bytes in %llu blocks\n", pid,
         leak_total_bytes, leak_total_blocks);
  fprintf(f, "==%d== \n", pid);

  fclose(f);
}


/* ----------- Local Functions ----------------------------------- */
static std::string addr_to_symbol(void *addr)
{
  std::string symbol;
  auto it = symbol_cache.find(addr);
  if (it != symbol_cache.end())
  {
    symbol = it->second;
  }
  else
  {
    Dl_info dlinfo;
    if (dladdr(addr, &dlinfo) && dlinfo.dli_sname)
    {
      if (dlinfo.dli_sname[0] == '_')
      {
        int status = -1;
        char *demangled = NULL;
        demangled = abi::__cxa_demangle(dlinfo.dli_sname, NULL, 0, &status);
        if (demangled)
        {
          if (status == 0)
          {
              symbol = std::string(demangled);
          }
          free(demangled);
        }
      }

      if (symbol.empty())
      {
        symbol = std::string(dlinfo.dli_sname);
      }

      if (!symbol.empty())
      {
        symbol += std::string(" + ");
        symbol += std::string(std::to_string((char*)addr - (char*)dlinfo.dli_saddr));
      }
    }

    symbol_cache[addr] = symbol;
  }

  return symbol;
}

