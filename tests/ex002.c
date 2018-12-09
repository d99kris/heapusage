/*
 * ex002.c
 *
 * Copyright (C) 2018 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Includes ------------------------------------------ */
#include <stdlib.h>


/* ----------- Global Functions ---------------------------------- */
int main(void)
{
  /* Allocate 1 block of 5555 bytes */
  void* ptr = malloc(5555);
  if (ptr == NULL)
  {
    return 1;
  }

  /* Free allocation */
  free(ptr);

  /* Free allocation again (double free) */
  free(ptr);

  return 0;
}

