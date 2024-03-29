/******************************************************************************
 *
 * CAEN SpA - Front End Division
 * Via Vetraia, 11 - 55049 - Viareggio ITALY
 * +390594388398 - www.caen.it
 *
 ***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#ifndef _CONFIGURE_H
#define _CONFIGURE_H  // Protect against multiple inclusion

#include "BoardUtils.h"
#include "CAENDigitizer.h"

//****************************************************************************
// Function prototypes
//****************************************************************************
int ProgramDigitizer(int brd, Config_t &WDcfg, const int handle[]);
int SetVirtualProbes(int brd, Config_t WDcfg, const int handle[]);
int SetTraceNames(Config_t WDcfg, int TraceSet[],
                  char TraceNames[][MAX_NTRSETS][20]);
#endif
