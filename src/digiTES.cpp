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

// This progam can be dowloaded from
// https://cloud.caen.it/public.php?service=files&t=72fd55f58d46c327637577f4fef11e73

#define digiTES_Revision "4.5.10"  // 22_December_2017

#include "digiTES.h"

#include <CAENDigitizer.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "BoardUtils.h"
#include "Configure.h"
#include "Console.h"
#include "ParamParser.h"

/* ###########################################################################
 *  Global Variables
 *  ###########################################################################
 */
// Config_t WDcfg;     // acquisition parameters and user settings
Stats_t Stats;    // variables for the statistics
Histos_t Histos;  // histograms (spectra)
// SysVars_t SysVars;  // system variables

int Quit = 0;
int AcqRun = 0;
int Stopping = 0;
int Failure = 0;
int RestartAll = 0;
int OvwrRunNumber = -1;  // Overwrite Run number
int CalibrRun = 0;       // ZC calibration Run
int ChToPlot = 0, BrdToPlot = 0;
int ContinuousTrigger = 0;
int SingleTrigger = 0;
int DoRefresh = 1;
int DoRefreshSingle = 0;
int StatsMode = 0;
int IntegratedRates = 0;
int SkipCalibration = 0;
int HistoPlotType = HPLOT_ENERGY;
int WavePlotType = WPLOT_DISABLED;
int ForceWaveMode = 0;
int Xunits = 0;
int handle[MAX_NBRD];  // board handles
int StopCh[MAX_NBRD]
          [MAX_NCH];  // Individual Stop Acquisition (based on time or counts)
char ConfigFileName[500];  // Config file name

int TraceSet[MAX_NTRACES];
char TraceNames[MAX_NTRACES][MAX_NTRSETS][20];

FILE *cntlog = NULL;
FILE *MsgLog = NULL;

#define MAX_NUM_CFG_FILES 50

int LoadSysVars(SysVars_t &SysVars)
{
  FILE *sv;
  char varname[100];

  // set defaults
  SysVars.CheckHeaderErrorBit = 0;
  SysVars.MaxOutputFileDataSize = 1024;  // 1 GB
  SysVars.FineTstampMode = 1;
  SysVars.UseRollOverFakeEvents = 1;
  SysVars.InactiveTimeout = 1000;  // ms
  SysVars.ZCcorr_Ncnt = 1024 * 256;
  SysVars.AutoReloadPlotsettings = 0;
  SysVars.ImmediateStart = 1;
  SysVars.AutoRestartOnCfgChange = 1;
  SysVars.HistoAutoSave = 60;  // save every minute
  SysVars.HVmax = 5100;
  sprintf(SysVars.ZCcalibrFileName, "%szcc.dat", WORKING_DIR);
  sprintf(SysVars.GnuplotCmd, "%s", DEFAULT_GNUPLOT_PATH);
  sv = fopen("sysvars.txt", "r");
  if (sv == NULL) return 0;

  msg_printf(MsgLog, "INFO: Reading system variables from 'sysvars.txt' \n");
  while (!feof(sv)) {
    fscanf(sv, "%s", varname);
    if (strcmp(varname, "UseRollOverFakeEvents") == 0)
      fscanf(sv, "%d", &SysVars.UseRollOverFakeEvents);
    if (strcmp(varname, "FineTstampMode") == 0)
      fscanf(sv, "%d", &SysVars.FineTstampMode);
    if (strcmp(varname, "CheckHeaderErrorBit") == 0)
      fscanf(sv, "%d", &SysVars.CheckHeaderErrorBit);
    if (strcmp(varname, "MaxOutputFileDataSize") == 0)
      fscanf(sv, "%d", &SysVars.MaxOutputFileDataSize);
    if (strcmp(varname, "ImmediateStart") == 0)
      fscanf(sv, "%d", &SysVars.ImmediateStart);
    if (strcmp(varname, "AutoRestartOnCfgChange") == 0)
      fscanf(sv, "%d", &SysVars.AutoRestartOnCfgChange);
    if (strcmp(varname, "AutoReloadPlotsettings") == 0)
      fscanf(sv, "%d", &SysVars.AutoReloadPlotsettings);
    if (strcmp(varname, "InactiveTimeout") == 0)
      fscanf(sv, "%d", &SysVars.InactiveTimeout);
    if (strcmp(varname, "HistoAutoSave") == 0)
      fscanf(sv, "%d", &SysVars.HistoAutoSave);
    if (strcmp(varname, "HVmax") == 0) fscanf(sv, "%d", &SysVars.HVmax);
    if (strcmp(varname, "ZCcorr_Ncnt") == 0)
      fscanf(sv, "%d", &SysVars.ZCcorr_Ncnt);
    if (strcmp(varname, "ZCcalibrFileName") == 0) {
      char fname[500];
      fscanf(sv, "%s", fname);
      sprintf(SysVars.ZCcalibrFileName, "%s%s", WORKING_DIR, fname);
    }
#ifdef WIN32
    if (strcmp(varname, "Windows_GnuplotCmd") == 0)
      fscanf(sv, "%s", SysVars.GnuplotCmd);
#else
    if (strcmp(varname, "Linux_GnuplotCmd") == 0)
      fscanf(sv, "%s", SysVars.GnuplotCmd);
#endif
  }
  fprintf(MsgLog, "INFO: System Variables (some of them):\n");
  fprintf(MsgLog, "INFO:   HistoAutoSave = %d\n", SysVars.HistoAutoSave);
  fprintf(MsgLog, "INFO:   MaxOutputFileDataSize = %d\n",
          SysVars.MaxOutputFileDataSize);
  fprintf(MsgLog, "INFO:   FineTstampMode = %d\n", SysVars.FineTstampMode);
  fprintf(MsgLog, "INFO:   UseRollOverFakeEvents = %d\n",
          SysVars.UseRollOverFakeEvents);
  fprintf(MsgLog, "INFO:   ZCcorr_Ncnt = %d\n", SysVars.ZCcorr_Ncnt);
  fprintf(MsgLog, "INFO:   ZCcalibrFileName = %s\n", SysVars.ZCcalibrFileName);
  fprintf(MsgLog, "INFO:   HVmax = %d\n", SysVars.HVmax);
  fprintf(MsgLog, "INFO:   GnuplotCmd = %s\n\n", SysVars.GnuplotCmd);
  return 0;
}
