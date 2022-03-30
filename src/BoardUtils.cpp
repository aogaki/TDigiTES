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

#include "BoardUtils.h"

#include <stdio.h>

// #include "Console.h"
#include "digiTES.h"
#ifdef WIN32
#include <process.h>
#include <sys/timeb.h>
#include <time.h>
#else
#include <ctype.h>  /* toupper() */
#include <stdint.h> /* C99 compliant compilers: uint64_t */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------------------------------------
// Description: Set bits of a registers
// Inputs:		handle = board handle
//				addr = address of the register
//				start_bit = first bit of the parameter being written
//				end_bit = last bit of the parameter being written
//				val: value to write
// Return:		0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int RegisterSetBits(int handle, uint16_t addr, int start_bit, int end_bit,
                    int val, Config_t &WDcfg)
{
  uint32_t mask = 0, reg;
  uint16_t ch;
  int ret;
  int i;

  if (((addr & 0xFF00) == 0x8000) && (addr != 0x8000) && (addr != 0x8004) &&
      (addr !=
       0x8008)) {  // broadcast access to channel individual registers (loop over channels)
    for (ch = 0; ch < WDcfg.NumPhyCh; ch++) {
      ret = CAEN_DGTZ_ReadRegister(handle, 0x1000 | (addr & 0xFF) | (ch << 8),
                                   &reg);
      for (i = start_bit; i <= end_bit; i++) mask |= 1 << i;
      reg = reg & ~mask | ((val << start_bit) & mask);
      ret |= CAEN_DGTZ_WriteRegister(handle, 0x1000 | (addr & 0xFF) | (ch << 8),
                                     reg);
    }
  } else {  // access to channel individual register or mother board register
    ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
    for (i = start_bit; i <= end_bit; i++) mask |= 1 << i;
    reg = reg & ~mask | ((val << start_bit) & mask);
    ret |= CAEN_DGTZ_WriteRegister(handle, addr, reg);
  }
  return ret;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Write to internal SPI bus
// Inputs:		handle = board handle
//				ch = channel
//				address = SPI address
//				value: value to write
// Return:		0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int WriteSPIRegister(int handle, uint32_t ch, uint32_t address, uint32_t value)
{
  uint32_t SPIBusy = 1;
  int32_t ret = CAEN_DGTZ_Success;

  uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
  uint32_t addressingRegAddr = 0x80B4;
  uint32_t valueRegAddr = 0x10B8 + (ch << 8);

  while (SPIBusy) {
    if ((ret = CAEN_DGTZ_ReadRegister(handle, SPIBusyAddr, &SPIBusy)) !=
        CAEN_DGTZ_Success)
      return CAEN_DGTZ_CommError;
    SPIBusy = (SPIBusy >> 2) & 0x1;
    if (!SPIBusy) {
      if ((ret = CAEN_DGTZ_WriteRegister(handle, addressingRegAddr, address)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
      if ((ret = CAEN_DGTZ_WriteRegister(handle, valueRegAddr, value)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
    }
    Sleep(1);
  }
  return CAEN_DGTZ_Success;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Read from internal SPI bus
// Inputs:		handle = board handle
//				ch = channel
//				address = SPI address
// Output		value: read value
// Return:		0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int ReadSPIRegister(int handle, uint32_t ch, uint32_t address, uint32_t *value)
{
  uint32_t SPIBusy = 1;
  int32_t ret = CAEN_DGTZ_Success;
  uint32_t SPIBusyAddr = 0x1088 + (ch << 8);
  uint32_t addressingRegAddr = 0x80B4;
  uint32_t valueRegAddr = 0x10B8 + (ch << 8);
  uint32_t val = 0x0000;

  while (SPIBusy) {
    if ((ret = CAEN_DGTZ_ReadRegister(handle, SPIBusyAddr, &SPIBusy)) !=
        CAEN_DGTZ_Success)
      return CAEN_DGTZ_CommError;
    SPIBusy = (SPIBusy >> 2) & 0x1;
    if (!SPIBusy) {
      if ((ret = CAEN_DGTZ_WriteRegister(handle, addressingRegAddr, address)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
      if ((ret = CAEN_DGTZ_ReadRegister(handle, valueRegAddr, value)) !=
          CAEN_DGTZ_Success)
        return CAEN_DGTZ_CommError;
    }
    Sleep(1);
  }
  return CAEN_DGTZ_Success;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Lock temperature compensation (i.e. disable dynamic calibration) of the ADC chips in
//              the x730 and x725 models
// Inputs:		handle = board handle
//				ch = channel
// Return:		0=OK, negative number = error code
// ---------------------------------------------------------------------------------------------------------
int LockTempCalibration_x730(int handle, int ch)
{
  uint32_t lock, ctrl;
  int ret = 0;

  // enter engineering functions
  ret |= WriteSPIRegister(handle, ch, 0x7A, 0x59);
  ret |= WriteSPIRegister(handle, ch, 0x7A, 0x1A);
  ret |= WriteSPIRegister(handle, ch, 0x7A, 0x11);
  ret |= WriteSPIRegister(handle, ch, 0x7A, 0xAC);

  // read lock value
  ret |= ReadSPIRegister(handle, ch, 0xA7, &lock);
  // write lock value
  ret |= WriteSPIRegister(handle, ch, 0xA5, lock);

  // enable lock
  ret |= ReadSPIRegister(handle, ch, 0xA4, &ctrl);
  ctrl |= 0x4;  // set bit 2
  ret |= WriteSPIRegister(handle, ch, 0xA4, ctrl);

  ret |= ReadSPIRegister(handle, ch, 0xA4, &ctrl);
  return ret;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Save all the regsiters of the borad to a file
// Inputs:		handle = handle of the board
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------------------------------
int SaveRegImage(int handle, Config_t &WDcfg)
{
  FILE *regs;
  char fname[100];
  int ret;
  uint32_t addr, reg, ch;

  sprintf(fname, "reg_image_%d.txt", handle);
  regs = fopen(fname, "w");
  if (regs == nullptr) return -1;

  fprintf(regs, "[COMMON REGS]\n");
  for (addr = 0x8000; addr <= 0x8200; addr += 4) {
    ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
    if (ret == 0) {
      fprintf(regs, "%04X : %08X\n", addr, reg);
    } else {
      fprintf(regs, "%04X : --------\n", addr);
      Sleep(1);
    }
  }
  for (addr = 0xEF00; addr <= 0xEF34; addr += 4) {
    ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
    if (ret == 0) {
      fprintf(regs, "%04X : %08X\n", addr, reg);
    } else {
      fprintf(regs, "%04X : --------\n", addr);
      Sleep(1);
    }
  }
  for (addr = 0xF000; addr <= 0xF088; addr += 4) {
    ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
    if (ret == 0) {
      fprintf(regs, "%04X : %08X\n", addr, reg);
    } else {
      fprintf(regs, "%04X : --------\n", addr);
      Sleep(1);
    }
  }

  for (ch = 0; ch < (uint32_t)WDcfg.NumPhyCh; ch++) {
    fprintf(regs, "[CHANNEL %d]\n", ch);
    for (addr = 0x1000 + (ch << 8); addr <= (0x10FF + (ch << 8)); addr += 4) {
      if (addr != 0x1090 + (ch << 8)) {
        ret = CAEN_DGTZ_ReadRegister(handle, addr, &reg);
        if (ret == 0) {
          fprintf(regs, "%04X : %08X\n", addr, reg);
        } else {
          fprintf(regs, "%04X : --------\n", addr);
          Sleep(1);
        }
      }
    }
  }

  fclose(regs);
  return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Description: Read Board Information
// Inputs:		b=board index
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------------------------------
int ReadBoardInfo(int b, char *ConnectString, Config_t &WDcfg,
                  const int handle[])
{
  int ret, i;
  uint32_t d32;
  int MajorNumber, MinorNumber;
  CAEN_DGTZ_BoardInfo_t BoardInfo;  // struct with the board information

  /* Once we have the handler to the digitizer, we use it to call the other functions */
  ret = CAEN_DGTZ_GetInfo(handle[b], &BoardInfo);
  if (ret) {
    printf("ERROR: Can't read board info\n");
    return -1;
  }

  WDcfg.NumPhyCh = BoardInfo.Channels;
  WDcfg.EnableMask[b] = 0;
  for (i = 0; i < MAX_NCH; i++)
    if ((WDcfg.EnableInput[b][i] << i) && (i < WDcfg.NumPhyCh))
      WDcfg.EnableMask[b] |= (1 << i);

  sprintf(ConnectString, "%s, %d, %d, ", BoardInfo.ModelName,
          BoardInfo.Channels, BoardInfo.SerialNumber);
  if (BoardInfo.FamilyCode == 5) {
    WDcfg.DigitizerModel = 751;
    WDcfg.Tsampl = 1;
    WDcfg.Nbit = 10;
  } else if (BoardInfo.FamilyCode == 7) {
    WDcfg.DigitizerModel = 780;
    WDcfg.Tsampl = 10;
    WDcfg.Nbit = 14;
  } else if (BoardInfo.FamilyCode == 13) {
    WDcfg.DigitizerModel = 781;
    WDcfg.Tsampl = 10;
    WDcfg.Nbit = 14;
  } else if (BoardInfo.FamilyCode == 0) {
    WDcfg.DigitizerModel = 724;
    WDcfg.Tsampl = 10;
    WDcfg.Nbit = 14;
  } else if (BoardInfo.FamilyCode == 11) {
    WDcfg.DigitizerModel = 730;
    WDcfg.Tsampl = 2;
    WDcfg.Nbit = 14;
  } else if (BoardInfo.FamilyCode == 14) {
    WDcfg.DigitizerModel = 725;
    WDcfg.Tsampl = 4;
    WDcfg.Nbit = 14;
  } else if (BoardInfo.FamilyCode == 3) {
    WDcfg.DigitizerModel = 720;
    WDcfg.Tsampl = 4;
    WDcfg.Nbit = 12;
  } else if (BoardInfo.FamilyCode == 999) {  // temporary code for Hexagon
    WDcfg.DigitizerModel = 5000;
    WDcfg.Tsampl = 10;
    WDcfg.Nbit = 14;
  } else {
    printf("ERROR: Unknown digitizer model\n");
    return -1;
  }

  /* Check firmware revision (only DPP firmware can be used with this Demo) */
  sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
  sscanf(&BoardInfo.AMC_FirmwareRel[4], "%d", &MinorNumber);
  WDcfg.FWrev = MinorNumber;
  if (WDcfg.DigitizerModel == 5000) {  // Hexagon
    sprintf(WDcfg.FwTypeString, "DPP_PHA_Hexagon");
    WDcfg.DppType = DPP_nPHA_724;
    strcat(ConnectString, "DPP_PHA_Hexagon, ");
  } else if (MajorNumber == 128) {
    if (MinorNumber >= 0x40) {
      sprintf(WDcfg.FwTypeString, "DPP_PHA (new version)");
      WDcfg.DppType = DPP_nPHA_724;
      strcat(ConnectString, "DPP_nPHA_724, ");
    } else {
      sprintf(WDcfg.FwTypeString, "DPP_PHA");
      WDcfg.DppType = DPP_PHA_724;
      strcat(ConnectString, "DPP_PHA_724, ");
    }
  } else if (MajorNumber == 130) {
    sprintf(WDcfg.FwTypeString, "DPP_CI");
    WDcfg.DppType = DPP_CI;
    strcat(ConnectString, "DPP_CI_720, ");
  } else if (MajorNumber == 131) {
    sprintf(WDcfg.FwTypeString, "DPP_PSD");
    WDcfg.DppType = DPP_PSD_720;
    strcat(ConnectString, "DPP_PSD_720, ");
  } else if (MajorNumber == 132) {
    sprintf(WDcfg.FwTypeString, "DPP_PSD");
    WDcfg.DppType = DPP_PSD_751;
    strcat(ConnectString, "DPP_PSD_751, ");
  } else if (MajorNumber == 136) {
    sprintf(WDcfg.FwTypeString, "DPP_PSD");
    WDcfg.DppType = DPP_PSD_730;  // NOTE: valid also for x725
    strcat(ConnectString, "DPP_PSD_730, ");
  } else if (MajorNumber == 139) {
    sprintf(WDcfg.FwTypeString, "DPP_PHA");
    WDcfg.DppType = DPP_PHA_730;  // NOTE: valid also for x725
    strcat(ConnectString, "DPP_PHA_730, ");
  } else {
    sprintf(WDcfg.FwTypeString, "Raw Waveform (Std. FW)");
    WDcfg.DppType = STD_730;
    WDcfg.AcquisitionMode = ACQ_MODE_MIXED;
    WDcfg.WaveformEnabled = 1;
    strcat(ConnectString, "Std FW, ");
  }
  printf("INFO: Brd %d: CAEN Digitizer Model %s (s.n. %d)\n", b,
         BoardInfo.ModelName, BoardInfo.SerialNumber);
  printf("INFO: ROC FPGA: %s\n", BoardInfo.ROC_FirmwareRel);
  printf("INFO: AMC FPGA: %s (%s)\n", BoardInfo.AMC_FirmwareRel,
         WDcfg.FwTypeString);
  sprintf(ConnectString, "%s %s, %s, ", ConnectString,
          BoardInfo.ROC_FirmwareRel, BoardInfo.AMC_FirmwareRel);

  if (IS_DPP_FW(WDcfg.DppType) && (WDcfg.DigitizerModel != 5000)) {
    CAEN_DGTZ_ReadRegister(handle[b], 0x8158, &d32);
    if (d32 == 0x53D4) {
      printf("INFO: The DPP is licensed\n");
      strcat(ConnectString, "YES");
    } else {
      if (d32 > 0) {
        printf("WARNING: DPP not licensed: %d minutes remaining\n",
               (int)((float)d32 / 0x53D4 * 30));
      } else {
        printf("ERROR: DPP not licensed: time expired\n\n");
        return -1;
      }
      strcat(ConnectString, "NO");
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Description: wait until the acquisition is started
// Return:		1=Acquisition Started, 0=Acquisition not started -1=error
// ---------------------------------------------------------------------------------------------------------
int WaitForAcquisitionStarted(int b, const int handle[])
{
  int ret = 0;
  uint32_t d32;

  do {
    ret = CAEN_DGTZ_ReadRegister(handle[b], CAEN_DGTZ_ACQ_STATUS_ADD,
                                 &d32);  // Read run status
    if (ret < 0) return ret;
    if (kbhit()) {
      getch();
      break;
    }
    Sleep(1);
  } while ((d32 & 0x4) == 0);
  if (d32 & 0x4)
    return 1;
  else
    return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Description: start the acquisition
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------------------------------
int StartAcquisition(Config_t &WDcfg, const int handle[])
{
  int b;
  int Started = 0;
  uint32_t d32a[MAX_NBRD], d32b[MAX_NBRD];
  time_t timer;
  struct tm *tm_info;

  // StartMode 1: use the TRGIN-TRGOUT daisy chain; the 1st trigger starts the acquisition
  if ((WDcfg.StartMode == START_MODE_TRGIN_1ST_SW) ||
      ((WDcfg.StartMode == START_MODE_TRGIN_1ST_HW))) {
    for (b = 0; b < WDcfg.NumBrd; b++) {
      uint32_t mask = (b == 0) ? 0x80000000 : 0x40000000;
      CAEN_DGTZ_ReadRegister(handle[b], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
                             &d32a[b]);
      CAEN_DGTZ_ReadRegister(handle[b], CAEN_DGTZ_FP_TRIGGER_OUT_ENABLE_ADD,
                             &d32b[b]);
      CAEN_DGTZ_WriteRegister(handle[b], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
                              mask);
      CAEN_DGTZ_WriteRegister(handle[b], CAEN_DGTZ_FP_TRIGGER_OUT_ENABLE_ADD,
                              mask);
    }

    if (WDcfg.StartMode == START_MODE_TRGIN_1ST_HW) {
      printf(
          "Boards armed. Waiting for TRGIN signal to start run (press a key to "
          "force)\n");
      Started = WaitForAcquisitionStarted(WDcfg.NumBrd - 1, handle);
    }
    if (!Started)
      CAEN_DGTZ_SendSWtrigger(
          handle
              [0]); /* Send a software trigger to the 1st board to start the acquisition */

    /* set the registers back to the original settings */
    /*for(b=0; b<WDcfg.NumBrd; b++) {
            CAEN_DGTZ_WriteRegister(handle[b], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD, d32a[b]);
            CAEN_DGTZ_WriteRegister(handle[b], CAEN_DGTZ_FP_TRIGGER_OUT_ENABLE_ADD, d32b[b]);
    }*/

    // StartMode 2: use SIN-TRGOUT daisy chain; the 1st board waits for an external logic level to start the acquisition
  } else if ((WDcfg.StartMode == START_MODE_SYNCIN_1ST_SW) ||
             (WDcfg.StartMode == START_MODE_SYNCIN_1ST_HW) ||
             (WDcfg.StartMode == START_MODE_SLAVE)) {
    if (WDcfg.StartMode == START_MODE_SYNCIN_1ST_HW ||
        (WDcfg.StartMode == START_MODE_SLAVE)) {
      printf(
          "Boards armed. Waiting for SIN/GPI signal to start run (press a key "
          "to force)\n");
      Started = WaitForAcquisitionStarted(WDcfg.NumBrd - 1, handle);
      printf("Start done\n");
    }
    if (!Started && (WDcfg.StartMode != START_MODE_SLAVE)) {
      uint32_t d32;
      //CAEN_DGTZ_SetAcquisitionMode(handle[0], CAEN_DGTZ_SW_CONTROLLED);
      CAEN_DGTZ_ReadRegister(handle[0], CAEN_DGTZ_ACQ_CONTROL_ADD, &d32);
      CAEN_DGTZ_WriteRegister(handle[0], CAEN_DGTZ_ACQ_CONTROL_ADD,
                              (d32 & 0xFFFFFFF8) | 0);  // SW controlled
      CAEN_DGTZ_SWStartAcquisition(handle[0]);
    }

    // StartMode 3: start the boards one by one with a software command (the time stamps of the boards won't be synchronized)
  } else {
    for (b = 0; b < WDcfg.NumBrd; b++) CAEN_DGTZ_SWStartAcquisition(handle[b]);
  }

  // string with start time and date
  time(&timer);
  tm_info = localtime(&timer);

  return 0;
}

// ---------------------------------------------------------------------------------------------------------
// Description: stop the acquisition
// Return:		0=OK, -1=error
// ---------------------------------------------------------------------------------------------------------
int StopAcquisition(Config_t &WDcfg, const int handle[])
{
  int b;
  // Note: in case the SIN-TRGOUT daisy chain is used to start/stop the run, stopping the 1st board will stop the acquisition in all of the boards
  // simultaneously; however, the CAEN_DGTZ_SWStopAcquisition function must be callod for the other boards too because they need to be disarmed
  printf("INFO: Stopping Acquisition\n");

  if ((WDcfg.StartMode == START_MODE_SYNCIN_1ST_SW) ||
      (WDcfg.StartMode == START_MODE_SYNCIN_1ST_HW)) {
    CAEN_DGTZ_SWStopAcquisition(handle[0]);
  } else if ((WDcfg.StartMode == START_MODE_SLAVE)) {
    ;
  } else {
    for (b = 0; b < WDcfg.NumBrd; b++) CAEN_DGTZ_SWStopAcquisition(handle[b]);
  }

  return 0;
}

// ---------------------------------------------------------------------------------------------------------
//  GETCH
// ---------------------------------------------------------------------------------------------------------
int getch(void)
{
  unsigned char temp;

  /* stdin = fd 0 */
  if (read(0, &temp, 1) != 1) return 0;
  return temp;
}

// ---------------------------------------------------------------------------------------------------------
//  KBHIT
// ---------------------------------------------------------------------------------------------------------
int kbhit()
{
  struct timeval timeout;
  fd_set read_handles;
  int status;

  /* check stdin (fd 0) for activity */
  FD_ZERO(&read_handles);
  FD_SET(0, &read_handles);
  timeout.tv_sec = timeout.tv_usec = 0;
  status = select(0 + 1, &read_handles, nullptr, nullptr, &timeout);
  if (status < 0) {
    printf("select() failed in kbhit()\n");
    exit(1);
  }
  return (status);
}
