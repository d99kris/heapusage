/*
 * huapi.cpp
 *
 * Copyright (C) 2017-2021 Kristofer Berggren
 * All rights reserved.
 *
 * heapusage is distributed under the BSD 3-Clause license, see LICENSE for details.
 *
 */


/* ----------- Includes ------------------------------------------ */
#include "hulog.h"


/* ----------- Global Functions ---------------------------------- */

extern "C" void hu_report()
{
  log_message("hu_report: Request Log Summary\n");
  log_summary_safe();
}
