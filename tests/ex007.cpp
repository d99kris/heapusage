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

#include "heapusage.h"

int main()
{
  char* a = (char*)malloc(1111);

  hu_report();

  char* b = (char*)malloc(2222);

  return (a != nullptr) && (b != nullptr);
}
