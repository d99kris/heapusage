/*
 * ex005.cpp
 *
 * Copyright (C) 2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main()
{
  char* s = (char*)malloc(8);
  free(s);

  // use-after-free write
  strcpy(s, "hello world");

  // use-after-free read
  char c = s[10]; // 'd'
  assert(c >= 0);
  
  return 0;
}
