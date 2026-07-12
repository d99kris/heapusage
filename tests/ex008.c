/*
 * ex008.c
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
#define SCRIBBLE_FREE_BYTE 0x55


/* ----------- Global Functions ---------------------------------- */
int main(void)
{
  int rv = 0;

  /* Allocate a block but do not initialize it. With the scribble tool enabled
     the freshly allocated memory is expected to be filled with 0xAA. */
  unsigned char* ptr = (unsigned char*)malloc(ALLOC_SIZE);
  if (ptr == NULL)
  {
    return 1;
  }

  /* Verify allocation scribble pattern (0xAA) */
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
  rv |= !alloc_ok;

  /* Free the block. With the scribble tool enabled the free'd memory is
     expected to be filled with 0x55. */
  free(ptr);

  /* Verify free scribble pattern (0x55) via a deliberate use-after-free read.
     The head of the block may hold allocator free-list metadata, so bytes are
     only checked past a small offset. */
  int free_ok = 1;
  for (int i = 64; i < ALLOC_SIZE; ++i)
  {
    if (ptr[i] != SCRIBBLE_FREE_BYTE)
    {
      free_ok = 0;
      break;
    }
  }
  printf("scribble free pattern %s\n", free_ok ? "ok" : "fail");
  rv |= !free_ok;

  return rv;
}
