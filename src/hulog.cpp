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
#include <libgen.h>
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

#if defined(__APPLE__)
#define _XOPEN_SOURCE 0
#define _POSIX_C_SOURCE 0
#endif
#include "backward.hpp"

#include "hulog.h"
#include "humain.h"


/* ----------- Defines ------------------------------------------- */
#define MAX_CALL_STACK 20   /* Limits the callstack depth to store */


/* ----------- Types --------------------------------------------- */
typedef struct hu_allocinfo_s
{
  void *ptr;
  size_t size;
  void *callstack[MAX_CALL_STACK];
  int callstack_depth;
  void *free_callstack[MAX_CALL_STACK];
  int free_callstack_depth;
  int count;
} hu_allocinfo_t;


/* ----------- File Global Variables ----------------------------- */
static pid_t pid = 0;
static char *hu_log_file = NULL;
static int hu_log_free = 0;
static int hu_log_nosyms = 0;
static size_t hu_log_minleak = 0;
static bool hu_useafterfree = false;
static bool hu_leak = false;

static long hu_page_size = 0;
static int logging_enabled = 0;

static unsigned long long allocinfo_total_frees = 0;
static unsigned long long allocinfo_total_allocs = 0;
static unsigned long long allocinfo_total_alloc_bytes = 0;
static unsigned long long allocinfo_current_alloc_bytes = 0;
static unsigned long long allocinfo_peak_alloc_bytes = 0;

static std::map<void*, hu_allocinfo_t>* allocations = nullptr;
static std::map<void*, hu_allocinfo_t>* freed_allocations = nullptr;
static std::map<void*, std::string>* symbol_cache = nullptr;
static std::map<void*, std::string>* objfile_cache = nullptr;


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
void log_init(char* file, bool doublefree, bool nosyms, size_t minsize, bool useafterfree,
              bool leak)
{
  /* Config */
  hu_log_file = file;
  hu_log_free = doublefree;
  hu_log_nosyms = nosyms;
  hu_log_minleak = minsize;
  hu_useafterfree = useafterfree;
  hu_leak = leak;

  /* Get runtime info */  
  pid = getpid();
  hu_page_size = sysconf(_SC_PAGE_SIZE);

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

  allocations = new std::map<void*, hu_allocinfo_t>();
  freed_allocations = new std::map<void*, hu_allocinfo_t>();
  symbol_cache = new std::map<void*, std::string>();
  objfile_cache = new std::map<void*, std::string>();
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

bool log_is_valid_callstack(int callstack_depth, void * const callstack[], bool is_alloc)
{
  int i = callstack_depth - 1;
  std::string objfile;
  while (i > 0)
  {
    void *addr = callstack[i];

    auto it = objfile_cache->find(addr);
    if (it != objfile_cache->end())
    {
      objfile = it->second;
    }
    else
    {
      Dl_info dlinfo;
      if (dladdr(addr, &dlinfo) && dlinfo.dli_fname)
      {
        char *fname = strdup(dlinfo.dli_fname);
        if (fname)
        {
          objfile = std::string(basename(fname));
          (*objfile_cache)[addr] = objfile;
          free(fname);
        }
      }
    }

    if (!objfile.empty())
    {
      // For now only care about originating object file
      break;
    }
      
    --i;
  }

  if (!objfile.empty())
  {
      // ignore invalid dealloc from libobjc
      if (!is_alloc && (objfile == "libobjc.A.dylib")) return false;
  }

  return true;
}

void log_event(int event, void *ptr, size_t size)
{
  if (logging_enabled)
  {
    if (event == EVENT_MALLOC)
    {
      if (hu_log_free)
      {
        freed_allocations->erase(ptr);
      }
      
      if (size >= hu_log_minleak)
      {
        hu_allocinfo_t allocinfo;
        allocinfo.size = size;
        allocinfo.ptr = ptr;
        allocinfo.callstack_depth = backtrace(allocinfo.callstack, MAX_CALL_STACK);
        allocinfo.count = 1;
        (*allocations)[ptr] = allocinfo;
        
        allocinfo_total_allocs += 1;
        allocinfo_total_alloc_bytes += size;
        allocinfo_current_alloc_bytes += size;

        if (allocinfo_current_alloc_bytes > allocinfo_peak_alloc_bytes)
        {
          allocinfo_peak_alloc_bytes = allocinfo_current_alloc_bytes;
        }
      }
    }
      else if (event == EVENT_FREE)
      {
        std::map<void*, hu_allocinfo_t>::iterator allocation = allocations->find(ptr);
        if (allocation != allocations->end())
        {
          allocinfo_current_alloc_bytes -= allocation->second.size;

          if (hu_useafterfree || hu_log_free)
          {
            allocation->second.free_callstack_depth =
              backtrace(allocation->second.free_callstack, MAX_CALL_STACK);
            freed_allocations->insert(*allocation);
          }
          
          allocations->erase(ptr);
        }
        else if (hu_log_free)
        {
          allocation = freed_allocations->find(ptr);
          if (allocation != freed_allocations->end())
          {
            void *callstack[MAX_CALL_STACK];
            int callstack_depth = backtrace(callstack, MAX_CALL_STACK);
            if (log_is_valid_callstack(callstack_depth, callstack, false))
            {
              FILE *f = fopen(hu_log_file, "a");
              if (f)
              {
                fprintf(f, "==%d== Invalid deallocation at:\n", pid);

                log_print_callstack(f, callstack_depth, callstack);

                fprintf(f, "==%d==  Address %p is a block of size %ld free'd at:\n",
                        pid, ptr, allocation->second.size);
          
                log_print_callstack(f, allocation->second.free_callstack_depth,
                                    allocation->second.free_callstack);

                fprintf(f, "==%d==  Block was alloc'd at:\n", pid);
          
                log_print_callstack(f, allocation->second.callstack_depth,
                                    allocation->second.callstack);

                fprintf(f, "==%d== \n", pid);

                fclose(f);
              }
            }
          }
        }

        allocinfo_total_frees += 1;
      }

  }
}

void hu_sig_handler(int sig, siginfo_t* si, void* /*ucontext*/)
{
  if ((sig != SIGSEGV) && (sig != SIGBUS)) return;

  if (si->si_code != SEGV_ACCERR) return;
  
  hu_set_bypass(true);

  void* ptr = si->si_addr;  
  void *callstack[MAX_CALL_STACK];
  int callstack_depth = backtrace(callstack, MAX_CALL_STACK);
  if (log_is_valid_callstack(callstack_depth, callstack, false))
  {
    FILE *f = fopen(hu_log_file, "a");
    if (f)
    {
      fprintf(f, "==%d== Invalid memory access at:\n", pid);

      log_print_callstack(f, callstack_depth, callstack);

      bool found = false;
      std::map<void*, hu_allocinfo_t>::iterator allocation;
      allocation = allocations->lower_bound((char*)ptr + 1);
      if (allocation != allocations->begin())
      {
        --allocation;
        
        if ((ptr >= ((char*)allocation->second.ptr + allocation->second.size)) &&
            (ptr <= ((char*)allocation->second.ptr + allocation->second.size + hu_page_size)))
        {
          found = true;
          size_t offset = (char*)ptr - ((char*)allocation->second.ptr + allocation->second.size);

          fprintf(f, "==%d==  Address %p is %ld bytes after a block of size %ld alloc'd at:\n",
                  pid, ptr, offset, allocation->second.size);
          
          log_print_callstack(f, allocation->second.callstack_depth, allocation->second.callstack);
        }
      }

      allocation = freed_allocations->lower_bound((char*)ptr + 1);
      if (!found && (allocation != freed_allocations->begin()))
      {
        --allocation;
        
        if ((ptr >= ((char*)allocation->second.ptr + allocation->second.size)) &&
            (ptr <= ((char*)allocation->second.ptr + allocation->second.size + hu_page_size)))
        {
          found = true;
          size_t offset = (char*)ptr - ((char*)allocation->second.ptr + allocation->second.size);

          fprintf(f, "==%d==  Address %p is %ld bytes after a block of size %ld free'd at:\n",
                  pid, ptr, offset, allocation->second.size);
          
          log_print_callstack(f, allocation->second.free_callstack_depth,
                              allocation->second.free_callstack);
        }
        else if ((ptr >= ((char*)allocation->second.ptr)) &&
                 (ptr <= ((char*)allocation->second.ptr + allocation->second.size + hu_page_size)))
        {
          found = true;
          size_t offset = (char*)ptr - ((char*)allocation->second.ptr);

          fprintf(f, "==%d==  Address %p is %ld bytes inside a block of size %ld free'd at:\n",
                  pid, ptr, offset, allocation->second.size);
          
          log_print_callstack(f, allocation->second.free_callstack_depth,
                              allocation->second.free_callstack);
        }        
 
        fprintf(f, "==%d==  Block was alloc'd at:\n", pid);

        log_print_callstack(f, allocation->second.callstack_depth, allocation->second.callstack);
      }
      
      fprintf(f, "==%d== \n", pid);

      fclose(f);
    }
  }

  hu_set_bypass(false);
  
  exit(EXIT_FAILURE);
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
  for (auto it = allocations->begin(); it != allocations->end(); ++it)
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
  if (hu_leak)
  {
    for (auto it = allocations_by_size.rbegin(); (it != allocations_by_size.rend()) && (it->size >= hu_log_minleak); ++it)
    {
      if (log_is_valid_callstack(it->callstack_depth, it->callstack, true))
      {
        fprintf(f, "==%d== %zu bytes in %d block(s) are lost, originally allocated at:\n", pid, it->size, it->count);

        log_print_callstack(f, it->callstack_depth, it->callstack);
    
        fprintf(f, "==%d== \n", pid);
      }
    }
  }
  
  /* Output leak summary */
  fprintf(f, "==%d== LEAK SUMMARY:\n", pid);
  fprintf(f, "==%d==    definitely lost: %llu bytes in %llu blocks\n", pid,
         leak_total_bytes, leak_total_blocks);
  fprintf(f, "==%d== \n", pid);

  fclose(f);
}

void hu_log_remove_freed_allocation(void* ptr)
{
  if (!hu_log_free)
  {
    freed_allocations->erase(ptr);
  }
}


/* ----------- Local Functions ----------------------------------- */
static std::string addr_to_symbol(void *addr)
{
  std::string symbol;
  auto it = symbol_cache->find(addr);
  if (it != symbol_cache->end())
  {
    symbol = it->second;
  }
  else
  {
#if (BACKWARD_HAS_BFD == 1) || (BACKWARD_HAS_DW == 1) || (BACKWARD_HAS_DWARF == 1)
    backward::TraceResolver trace_resolver;
    trace_resolver.load_addresses(&addr, 1);
    backward::Trace trace(addr, 0);
    backward::ResolvedTrace rtrace = trace_resolver.resolve(trace);
    if (!rtrace.source.filename.empty())
    {
      const std::string& path = rtrace.source.filename;
      std::string filename = path.substr(path.find_last_of("/\\") + 1);
      symbol = rtrace.object_function +
        " (" + filename + ":" + std::to_string(rtrace.source.line) + ")";
    }
    else
    {
      symbol = rtrace.object_function;
    }
#else
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
#endif

    (*symbol_cache)[addr] = symbol;
  }

  return symbol;
}

