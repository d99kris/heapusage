/*
 * humain.h
 *
 * Copyright (C) 2017-2021 Kristofer Berggren
 * All rights reserved.
 * 
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */

/* ----------- Global Function Prototypes ------------------------ */
extern "C" void __attribute__ ((constructor)) hu_init(void);
extern "C" void __attribute__ ((destructor)) hu_fini(void);

void hu_set_bypass(bool bypass);
