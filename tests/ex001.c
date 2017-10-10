/*
 * ex001.c
 *
 * Copyright (C) 2017 Kristofer Berggren
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
  /* Allocate 1 block of 5555 bytes never free'd */
  void* ptr = malloc(5555);
  if (ptr == NULL)
  {
    return 1;
  }

  /* Allocate 3 blocks of 2222 bytes each never free'd */
  int a = 3;
  while(a-- > 0)
  {
    ptr = malloc(2222);
    if (ptr == NULL)
    {
      break;
    }
  }

  /* Allocate 1 block of 1111 bytes immediately free'd */
  free(malloc(1111));

  return 0;
}

