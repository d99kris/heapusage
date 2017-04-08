/*
 * hulog.h
 *
 * Copyright (C) 2017 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Defines ------------------------------------------- */
#define EVENT_MALLOC   1
#define EVENT_FREE     2


/* ----------- Global Function Prototypes ------------------------ */
#ifdef __cplusplus
extern "C" {
#endif

void log_init(void);
void log_enable(int flag);
void log_event(int event, void *ptr, size_t size);
void log_summary(void);

#ifdef __cplusplus
}
#endif

