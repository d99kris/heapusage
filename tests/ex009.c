/*
 * ex009.c
 *
 * Copyright (C) 2026 Kristofer Berggren
 * All rights reserved.
 *
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>


/* ----------- Defines ------------------------------------------- */
#define ALLOC_SIZE 256
#define SCRIBBLE_ALLOC_BYTE 0xAA


/* ----------- Global Functions ---------------------------------- */
int main(void)
{
  /* Allocate a block but do not initialize it. With the scribble tool enabled
     (as part of the "all" tool set) the freshly allocated memory is expected to
     be filled with 0xAA. Only the allocation pattern is checked here so the
     program does not access free'd memory, which would trip the guard-page
     based use-after-free detection also enabled by "all". */
  unsigned char* ptr = (unsigned char*)malloc(ALLOC_SIZE);
  if (ptr == NULL)
  {
    return 1;
  }

  int alloc_ok = 1;
  for (int i = 0; i < ALLOC_SIZE; ++i)
  {
    if (ptr[i] != SCRIBBLE_ALLOC_BYTE)
    {
      alloc_ok = 0;
      break;
    }
  }
  printf("scribble alloc pattern %s\n", alloc_ok ? "ok" : "fail");

  free(ptr);

  return alloc_ok ? 0 : 1;
}
