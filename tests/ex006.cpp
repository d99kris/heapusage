/*
 * ex006.cpp
 *
 * Copyright (C) 2024 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

#include <cstdlib>

#include <unistd.h>

int main()
{
  char* a = (char*)malloc(1111);

  sleep(2);

  char* b = (char*)malloc(2222);

  return (a != nullptr) && (b != nullptr);
}
