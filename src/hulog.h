/*
 * hulog.h
 *
 * Copyright (C) 2017-2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <signal.h>


/* ----------- Defines ------------------------------------------- */
#define EVENT_MALLOC   1
#define EVENT_FREE     2


/* ----------- Global Function Prototypes ------------------------ */
void log_init(char* file, bool doublefree, bool nosyms, size_t minsize, bool useafterfree,
              bool leak);
void log_enable(int flag);
void log_event(int event, void* ptr, size_t size);
void log_invalid_access(void* ptr);
void hu_sig_handler(int sig, siginfo_t* si, void* /*ucontext*/);
void log_message(const char *format, ...);
void log_summary();
void log_summary_safe();
void hu_log_remove_freed_allocation(void* ptr);
