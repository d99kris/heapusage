/*
 * ex004.cpp
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
#include <iostream>

#if defined (__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

int main()
{
  char* s = (char*)malloc(16);

#if defined (__APPLE__)
  std::cout << "actual size " << malloc_size(s) << "\n";
#endif

  // heap-buffer-overflow write
  strcpy(s, "123456789ABCDEF0123");

  // heap-buffer-overflow read
  char c = s[16];
  assert(c >= 0);
  
  return 0;
}
