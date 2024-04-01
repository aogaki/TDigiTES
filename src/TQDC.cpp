#include "TQDC.hpp"

#include <CAENDigitizer.h>
#include <string.h>

#include <iostream>
#include <string>

TQDC::TQDC()
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpQDCWaveform[iBrd] = nullptr;
    // fppQDCEvents[iBrd] = nullptr;

    for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
      fPreviousTime[iBrd][iCh] = 0;
      fTimeOffset[iBrd][iCh] = 0;
      fLostTrgCounter[iBrd][iCh] = 0.;
      fLostTrgCounterOffset[iBrd][iCh] = 0;
    }
  }

  fFlagFineTS = false;
  fFlagHWFineTS = false;
  for (auto &&mod : fFlagTrgCounter) {
    for (auto &&ch : mod) {
      ch = false;
    }
  }

  fFlagExtTS = true;
}

TQDC::~TQDC() { FreeMemory(); }

void TQDC::InitDigitizers()
{
  GetBoardInfo();
  LoadQDCParameters();

  // Simple copy of sample code for DPP_QDC
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_Reset(fHandler[iBrd]);
    PrintError(err, "CAEN_DGTZ_Reset");

    // Ch and group mask
    uint32_t groupMask = 0;
    for (auto iGroup = 0; iGroup < nGroups; iGroup++) {
      uint32_t chMask = 0;
      auto start = iGroup * nChInGroup;
      auto stop = start + nChInGroup;
      for (auto iCh = start; iCh < stop; iCh++) {
        if (fWDcfg.EnableInput[iBrd][iCh] != 0) chMask |= (1 << (iCh - start));
      }

      err = CAEN_DGTZ_SetChannelGroupMask(fHandler[iBrd], iGroup, chMask);
      PrintError(err, "CAEN_DGTZ_SetChannelGroupMask");
      if (chMask != 0) groupMask |= (1 << iGroup);
    }
    err = CAEN_DGTZ_SetGroupEnableMask(fHandler[iBrd], groupMask);
    PrintError(err, "CAEN_DGTZ_SetGroupEnableMask");

    // Threshold settings
    for (auto iCh = 0; iCh < nChannels; iCh++) {
      err = CAEN_DGTZ_SetChannelTriggerThreshold(
          fHandler[iBrd], iCh, fWDcfg.TrgThreshold[iBrd][iCh]);
      PrintError(err, "CAEN_DGTZ_SetChannelTriggerThreshold");
    }

    // Generating the common trigger for the group?
    err = CAEN_DGTZ_SetGroupSelfTrigger(fHandler[iBrd],
                                        CAEN_DGTZ_TRGMODE_ACQ_ONLY, 0x00);
    PrintError(err, "CAEN_DGTZ_SetGroupSelfTrigger");

    err =
        CAEN_DGTZ_SetSWTriggerMode(fHandler[iBrd], CAEN_DGTZ_TRGMODE_ACQ_ONLY);
    PrintError(err, "CAEN_DGTZ_SetSWTriggerMode");

    err = CAEN_DGTZ_SetMaxNumAggregatesBLT(fHandler[iBrd], 1023);
    PrintError(err, "CAEN_DGTZ_SetMaxNumAggregatesBLT");

    err = CAEN_DGTZ_SetAcquisitionMode(fHandler[iBrd], CAEN_DGTZ_SW_CONTROLLED);
    PrintError(err, "CAEN_DGTZ_SetAcquisitionMode");

    std::cout << "Acq mode: " << fWDcfg.AcquisitionMode << std::endl;
    err = CAEN_DGTZ_SetDPPAcquisitionMode(
        fHandler[iBrd], (CAEN_DGTZ_DPP_AcqMode_t)fWDcfg.AcquisitionMode,
        CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime);
    PrintError(err, "CAEN_DGTZ_SetDPPAcquisitionMode");

    // Aogaki: Not checked after this line

    // TODO: Check -1 is working for QDC or not
    // Probably this does not support QDC
    // std::cout << fWDcfg.PreTrigger << std::endl;
    // err = CAEN_DGTZ_SetDPPPreTriggerSize(fHandler[iBrd], -1, fWDcfg.PreTrigger);
    // PrintError(err, "CAEN_DGTZ_SetDPPPreTriggerSize");
    for (auto iGrp = 0; iGrp < nGroups; iGrp++) {
      err = CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x103C + iGrp * 0x100,
                                    fWDcfg.PreTrigger);
      PrintError(err, "CAEN_DGTZ_WriteRegister");
    }

    err =
        CAEN_DGTZ_SetDPPParameters(fHandler[iBrd], groupMask, &fQDCParameters);
    PrintError(err, "CAEN_DGTZ_SetDPPParameters");

    auto recLength = fWDcfg.RecordLength / fTSample[iBrd];
    std::cout << "Record length: " << recLength << " samples x "
              << fTSample[iBrd] << " ns = " << recLength * fTSample[iBrd]
              << " ns" << std::endl;
    err = CAEN_DGTZ_SetRecordLength(fHandler[iBrd], recLength);

    for (auto iGroup = 0; iGroup < MAX_X740_GROUP_SIZE; iGroup++) {
      auto ratio = fWDcfg.DCoffset[iBrd][iGroup];
      if (fWDcfg.PulsePolarity[iBrd][iGroup] == 1) ratio = 100 - ratio;
      auto offset = 0xFFFF * ratio / 100;
      err = CAEN_DGTZ_SetGroupDCOffset(fHandler[iBrd], iGroup, offset);
    }

    err = CAEN_DGTZ_SetDPPEventAggregation(fHandler[iBrd],
                                           fWDcfg.EventBuffering, 0);

    RegisterSetBits(fHandler[iBrd], 0x8000, 17, 17, 1, fWDcfg);  // Enable Extra
  }

  SetFrontPanel();
}

void TQDC::SetFrontPanel()
{
  if (fWDcfg.TrgoutMode == TRGOUT_MODE_SYNC_OUT) {
    // for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    //   std::cout << "Synchronizqation setting " << iBrd << std::endl;
    //   CAEN_DGTZ_SetRunSynchronizationMode(
    //       fHandler[iBrd], CAEN_DGTZ_RUN_SYNC_TrgOutSinDaisyChain);
    //   if (iBrd == 0) {
    //     CAEN_DGTZ_SetAcquisitionMode(fHandler[iBrd], CAEN_DGTZ_SW_CONTROLLED);
    //   } else {
    //     CAEN_DGTZ_SetAcquisitionMode(fHandler[iBrd], CAEN_DGTZ_S_IN_CONTROLLED);
    //   }

    //   int err = 0;

    //   CAEN_DGTZ_RunSyncMode_t runMode;
    //   err = CAEN_DGTZ_GetRunSynchronizationMode(fHandler[iBrd], &runMode);
    //   std::cout << runMode << "\t" << err << std::endl;

    //   CAEN_DGTZ_AcqMode_t acqMode;
    //   err = CAEN_DGTZ_GetAcquisitionMode(fHandler[iBrd], &acqMode);
    //   std::cout << acqMode << "\t" << err << std::endl;
    // }
  }
  SetTRGIN();
  SetSIN();
  SetTRGOUT();
}

void TQDC::SetTRGIN()
{
  // CAEN_DGTZ_ErrorCode err;
  int err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fWDcfg.TrginMode == TRGIN_MODE_DISABLED) {
      err |= CAEN_DGTZ_WriteRegister(
          fHandler[iBrd], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
          0x80000000);  // accept SW trg only (ext trig is disabled)
    } else if (fWDcfg.TrginMode == TRGIN_MODE_GLOBAL_TRG) {
      err |= CAEN_DGTZ_WriteRegister(
          fHandler[iBrd], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
          0xC0000000);  // accept ext trg_in or SW trg
    } else if ((fWDcfg.TrginMode == TRGIN_MODE_VETO) ||
               (fWDcfg.TrginMode == TRGIN_MODE_GATE)) {
      err |= CAEN_DGTZ_WriteRegister(
          fHandler[iBrd], CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
          0x80000000);  // accept SW trg only (ext trig
                        // is used for the veto/gate)
      err |= RegisterSetBits(
          fHandler[iBrd], 0x811C, 10, 11, 3,
          fWDcfg);  // propagate ext-trg "as is" to channels (will be
                    // used as a validation for the self triggers)

    } else if (fWDcfg.TrginMode ==
               TRGIN_MODE_COINC) {  // TrgIn fan out to each channel (individual
                                    // trg validation)
      err |= CAEN_DGTZ_WriteRegister(fHandler[iBrd],
                                     CAEN_DGTZ_TRIGGER_SRC_ENABLE_ADD,
                                     0x80000000);  // accept SW trg only
      // for (auto i = 0; i < 8; i++)
      //   err |=
      //       CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8180 + i * 4, 0x40000000);
    }
  }
}

void TQDC::SetTRGOUT()
{
  int ret = 0;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fWDcfg.TrgoutMode == TRGOUT_MODE_DISABLED) {  // disabled (OR mask=0)
      ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8110, 0);
    } else if (fWDcfg.TrgoutMode ==
               TRGOUT_MODE_PROP_TRGIN) {  // propagate trgin
      ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8110, 0x40000000);
    } else if (fWDcfg.TrgoutMode ==
               TRGOUT_MODE_CH_TRG) {  // propagate self triggers (with mask)
      // ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8110, fWDcfg.TrgoutMask);
    } else if (fWDcfg.TrgoutMode ==
               TRGOUT_MODE_SYNC_OUT) {  // propagate sync signal (start/stop)
      ret |= RegisterSetBits(fHandler[iBrd], 0x811C, 16, 17, 0x1, fWDcfg);
      ret |= RegisterSetBits(fHandler[iBrd], 0x811C, 18, 19, 0x0, fWDcfg);
    } else if (fWDcfg.TrgoutMode == TRGOUT_CLOCK) {  // propagate internal clock
      ret |= CAEN_DGTZ_WriteRegister(
          fHandler[iBrd], CAEN_DGTZ_FRONT_PANEL_IO_CTRL_ADD, 0x00050000);
    } else if (fWDcfg.TrgoutMode == TRGOUT_SIGSCOPE) {  //
      // ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8110, fWDcfg.TrgoutMask);
    } else {  // Use TRGOUT/GPO as a test pulser
      ret |= RegisterSetBits(fHandler[iBrd], 0x811C, 15, 15, 1, fWDcfg);
      // if (fWDcfg.TrgoutMode == TRGOUT_MODE_SQR_1KHZ)
      //   ret |= RegisterSetBits(fHandler[iBrd], 0x8168, 0, 2, 1,
      //                          fWDcfg);  // 1 KHz square wave
      // if (fWDcfg.TrgoutMode == TRGOUT_MODE_PLS_1KHZ)
      //   ret |= RegisterSetBits(fHandler[iBrd], 0x8168, 0, 2, 2,
      //                          fWDcfg);  // 1 KHz pulses
      // if (fWDcfg.TrgoutMode == TRGOUT_MODE_SQR_10KHZ)
      //   ret |= RegisterSetBits(fHandler[iBrd], 0x8168, 0, 2, 3,
      //                          fWDcfg);  // 10 KHz square wave
      // if (fWDcfg.TrgoutMode == TRGOUT_MODE_PLS_10KHZ)
      //   ret |= RegisterSetBits(fHandler[iBrd], 0x8168, 0, 2, 4,
      //                          fWDcfg);  // 10 KHz pulses
    }
  }
}

void TQDC::SetSIN()
{
  int ret = 0;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    switch (fWDcfg.StartMode) {
      case START_MODE_INDEP_SW:
        ret |= CAEN_DGTZ_SetAcquisitionMode(fHandler[iBrd],
                                            CAEN_DGTZ_SW_CONTROLLED);
        break;
      case START_MODE_SYNCIN_1ST_SW:
      case START_MODE_SYNCIN_1ST_HW:
      case START_MODE_SLAVE:
        if (fWDcfg.SyncinMode != SYNCIN_MODE_RUN_CTRL) {
          printf(
              "WARNING: SyncinMode must be set as RUN_CTRL; forced option\n");
          fWDcfg.SyncinMode = SYNCIN_MODE_RUN_CTRL;
        }
        if (fWDcfg.TrgoutMode != TRGOUT_MODE_SYNC_OUT) {
          printf(
              "WARNING: TrgoutMode must be set as SYNC_OUT; forced option\n");
          fWDcfg.TrgoutMode = TRGOUT_MODE_SYNC_OUT;
        }
        uint32_t d32;
        ret |= CAEN_DGTZ_ReadRegister(fHandler[iBrd], CAEN_DGTZ_ACQ_CONTROL_ADD,
                                      &d32);
        ret |= CAEN_DGTZ_WriteRegister(
            fHandler[iBrd], CAEN_DGTZ_ACQ_CONTROL_ADD,
            (d32 & 0xFFFFFFF0) |
                RUN_START_ON_SIN_LEVEL);  // Arm acquisition (Run will start when
                                          // SIN goes high)
        // Run Delay to deskew the start of acquisition
        if (iBrd == 0 && fWDcfg.StartMode != START_MODE_SLAVE)
          ret |= CAEN_DGTZ_WriteRegister(
              fHandler[iBrd], 0x8170,
              (fWDcfg.NumBrd + fWDcfg.Slave - 1) * 3 + 1);
        else if (fWDcfg.StartMode == START_MODE_SLAVE)
          ret |= CAEN_DGTZ_WriteRegister(
              fHandler[iBrd], 0x8170,
              (fWDcfg.NumBrd + fWDcfg.Slave - iBrd - 1) * 3);
        else
          ret |= CAEN_DGTZ_WriteRegister(
              fHandler[iBrd], 0x8170,
              (fWDcfg.NumBrd + fWDcfg.Slave - iBrd - 1) * 3);
        break;
      case START_MODE_TRGIN_1ST_SW:
      case START_MODE_TRGIN_1ST_HW:
        ret |= CAEN_DGTZ_ReadRegister(fHandler[iBrd], CAEN_DGTZ_ACQ_CONTROL_ADD,
                                      &d32);
        ret |= CAEN_DGTZ_WriteRegister(
            fHandler[iBrd], CAEN_DGTZ_ACQ_CONTROL_ADD,
            (d32 & 0xFFFFFFF0) |
                RUN_START_ON_TRGIN_RISING_EDGE);  // Arm acquisition (Run will
                                                  // start with 1st trigger)
        // Run Delay to deskew the start of acquisition
        if (iBrd == 0)
          ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8170,
                                         (fWDcfg.NumBrd - 1) * 3 + 1);
        else
          ret |= CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8170,
                                         (fWDcfg.NumBrd - iBrd - 1) * 3);
        break;
    }

    // SyncinMode
    if (fWDcfg.SyncinMode == SYNCIN_MODE_DISABLED) {
      // there is not any way to disable SIN in the firmware!!!
    }
  }
}

void TQDC::LoadQDCParameters()
{
  std::cout << "Number of Boards: " << fWDcfg.NumBrd << std::endl;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (auto iGroup = 0; iGroup < MAX_X740_GROUP_SIZE; iGroup++) {
      std::cout << "Board: " << iBrd << " group: " << iGroup << std::endl;

      auto gateWidth = fWDcfg.GateWidth[iBrd][iGroup] / fTSample[iBrd];
      std::cout << "Gate width: " << gateWidth << " samples x "
                << fTSample[iBrd] << " ns = " << gateWidth * fTSample[iBrd]
                << " ns" << std::endl;
      fQDCParameters[iBrd].GateWidth[iGroup] = gateWidth;

      std::cout << "Self trigger ON" << std::endl;
      fQDCParameters[iBrd].DisTrigHist[iGroup] = 0;
      fQDCParameters[iBrd].DisSelfTrigger[iGroup] = 0;

      std::cout << "Baseline calculation samples: 4^"
                << fWDcfg.NsBaseline[iBrd][iGroup] << std::endl;
      if (fWDcfg.NsBaseline[iBrd][iGroup] == 0) {
        std::cout << "Zero level: " << fWDcfg.ZeroVoltLevel[iBrd][iGroup]
                  << std::endl;
      }
      fQDCParameters[iBrd].BaselineMode[iGroup] =
          fWDcfg.NsBaseline[iBrd][iGroup];
      fQDCParameters[iBrd].FixedBaseline[iGroup] =
          fWDcfg.ZeroVoltLevel[iBrd][iGroup];

      std::cout << "Trigger mode: " << fWDcfg.TrgMode
                << "\t (0: NORMAL, 1: PAIRED, 2: EXTERNAL)" << std::endl;
      fQDCParameters[iBrd].TrgMode[iGroup] = fWDcfg.TrgMode;

      std::cout << "Charge sensitivity: "
                << fWDcfg.ChargeSensitivity[iBrd][iGroup]
                << "\t(0 = 0.16 pC, 1 = 0.32 pC, 2 = 0.64 pC, 3 = 1.28 pC, 4 = "
                   "2.56 pC, 5 = 5.12 pC, 6 = 10.24 pC, 7 = 20.48 pC)"
                << std::endl;
      fQDCParameters[iBrd].ChargeSensitivity[iGroup] =
          fWDcfg.ChargeSensitivity[iBrd][iGroup];

      std::cout << "Pulse polarity: " << fWDcfg.PulsePolarity[iBrd][iGroup]
                << "\t(1 = Negative pulses; 0 = Positive Pulses)" << std::endl;
      fQDCParameters[iBrd].PulsePol[iGroup] =
          fWDcfg.PulsePolarity[iBrd][iGroup];

      std::cout << "Enable charge pedestal: Always false now" << std::endl;
      fQDCParameters[iBrd].EnChargePed[iGroup] = 0;

      // 0 = 1 KHz; 1 = 10 KHz; 2 = 100 KHz; 3 = 1 MHz
      fQDCParameters[iBrd].TestPulsesRate[iGroup] = 0;
      std::cout << "Enable test pulses: Always false now" << std::endl;
      fQDCParameters[iBrd].EnTestPulses[iGroup] = 0;

      std::cout << "Input smoothing: " << fWDcfg.TTFsmoothing[iBrd][iGroup]
                << "\t2^n samples, n <= 6" << std::endl;
      fQDCParameters[iBrd].InputSmoothing[iGroup] =
          fWDcfg.TTFsmoothing[iBrd][iGroup];

      auto preGate = fWDcfg.PreGate[iBrd][iGroup] / fTSample[iBrd];
      std::cout << "Pre gate: " << preGate << " samples x " << fTSample[iBrd]
                << " ns = " << preGate * fTSample[iBrd] << " ns" << std::endl;
      fQDCParameters[iBrd].PreGate[iGroup] = preGate;

      std::cout << "Extended time stamp is always ON" << std::endl;
      fQDCParameters[iBrd].EnableExtendedTimeStamp = 1;
      fFlagExtTS = true;

      auto trgHoldOff = fWDcfg.TrgHoldOff / fTSample[iBrd];
      std::cout << "Trigger hold off: " << trgHoldOff << " samples x "
                << fTSample[iBrd] << " ns = " << trgHoldOff * fTSample[iBrd]
                << " ns" << std::endl;
      fQDCParameters[iBrd].trgho[iGroup] = trgHoldOff;
      std::cout << std::endl;
    }
  }
}

void TQDC::Test() {}

void TQDC::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[iBrd],
                                        &(fpReadoutBuffer[iBrd]), &size);
    PrintError(err, "MallocReadoutBuffer");

    // CAEN_DGTZ_DPP_QDC_Event_t *events[fNChs];
    fppQDCEvents[iBrd] = new CAEN_DGTZ_DPP_QDC_Event_t *[MAX_NCH];
    err = CAEN_DGTZ_MallocDPPEvents(fHandler[iBrd],
                                    (void **)(fppQDCEvents[iBrd]), &size);
    PrintError(err, "MallocDPPEvents");

    err = CAEN_DGTZ_MallocDPPWaveforms(fHandler[iBrd],
                                       (void **)&(fpQDCWaveform[iBrd]), &size);
    PrintError(err, "MallocDPPWaveforms");
  }
}

void TQDC::FreeMemory()
{
  // In digiTes, fppQDCEvents is not the array.
  // I have to check the double freeing memory
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      PrintError(err, "FreeReadoutBuffer");
      fpReadoutBuffer[iBrd] = nullptr;
    }
    if ((void **)fppQDCEvents[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[iBrd],
                                    (void **)(fppQDCEvents[iBrd]));
      PrintError(err, "FreeDPPEvents");
      fppQDCEvents[iBrd] = nullptr;
    }
    if (fpQDCWaveform[iBrd] != nullptr) {
      // CAENlib has malloc but NOT free for QDC FW.
      // err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[iBrd],
      //                                  (void *)&(fpQDCWaveform[iBrd]));
      // PrintError(err, "FreeDPPWaveforms");
      // free(fpQDCWaveform[iBrd]->Trace1);
      delete fpQDCWaveform[iBrd]->Trace1;
      fpQDCWaveform[iBrd]->Trace1 = nullptr;
      delete fpQDCWaveform[iBrd]->Trace2;
      fpQDCWaveform[iBrd]->Trace2 = nullptr;
      delete fpQDCWaveform[iBrd]->DTrace1;
      fpQDCWaveform[iBrd]->DTrace1 = nullptr;
      delete fpQDCWaveform[iBrd]->DTrace2;
      fpQDCWaveform[iBrd]->DTrace2 = nullptr;
      delete fpQDCWaveform[iBrd]->DTrace3;
      fpQDCWaveform[iBrd]->DTrace3 = nullptr;
      delete fpQDCWaveform[iBrd]->DTrace4;
      fpQDCWaveform[iBrd]->DTrace4 = nullptr;

      delete fpQDCWaveform[iBrd];
      fpQDCWaveform[iBrd] = nullptr;
    }
  }
}

void TQDC::ReadSmallData() {}

void TQDC::ReadRawData()
{
  RawData_t rawData(new std::vector<std::vector<char>>);

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    uint32_t bufferSize;
    auto err = CAEN_DGTZ_ReadData(fHandler[iBrd],
                                  CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                                  fpReadoutBuffer[iBrd], &bufferSize);
    PrintError(err, "ReadData");

    // 0 event also used to check the board has event or not
    std::vector<char> v(bufferSize);
    if (bufferSize > 0) memcpy(&v[0], fpReadoutBuffer[iBrd], bufferSize);
    rawData->push_back(v);
  }

  fMutex.lock();
  fRawDataQue.push_back(rawData);
  fMutex.unlock();
}

void TQDC::DecodeRawData()
{
  fMutex.lock();
  const auto nRawData = fRawDataQue.size();
  fMutex.unlock();

  if (nRawData > 0) {
    fMutex.lock();
    auto rawData = fRawDataQue.front();
    fRawDataQue.pop_front();
    fMutex.unlock();

    for (auto iBrd = 0; iBrd < rawData->size(); iBrd++) {
      uint32_t bufferSize = rawData->at(iBrd).size();

      if (bufferSize == 0)
        continue;  // in the case of 0, GetDPPEvents makes crush

      uint32_t nEvents[MAX_NCH];
      auto err = CAEN_DGTZ_GetDPPEvents(fHandler[iBrd], &(rawData->at(iBrd)[0]),
                                        bufferSize,
                                        (void **)(fppQDCEvents[iBrd]), nEvents);
      PrintError(err, "GetDPPEvents");

      if (err == CAEN_DGTZ_Success) {
        // for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
        for (uint iCh = 0; iCh < 64; iCh++) {
          for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
            err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                               &(fppQDCEvents[iBrd][iCh][iEve]),
                                               fpQDCWaveform[iBrd]);
            PrintError(err, "DecodeDPPWaveforms");

            auto data = std::make_shared<TreeData_t>(fpQDCWaveform[iBrd]->Ns);
            data->Mod = iBrd + fStartMod;
            data->Ch = iCh;
            data->ChargeLong = fppQDCEvents[iBrd][iCh][iEve].Charge;
            data->ChargeShort = 0;
            data->RecordLength = fpQDCWaveform[iBrd]->Ns;
            data->Extras = fppQDCEvents[iBrd][iCh][iEve].Extras;

            uint64_t timeTag = fppQDCEvents[iBrd][iCh][iEve].TimeTag;
            if (fFlagHWFineTS || fFlagExtTS) {
              uint64_t extTS =
                  ((uint64_t)((data->Extras >> 16) & 0xFFFF) << 31);
              uint64_t tdc = (timeTag + extTS) * fWDcfg.Tsampl;
              data->TimeStamp = tdc;
            } else {
              const uint64_t TSMask =
                  (fWDcfg.DppType == DPP_PSD_751) ? 0xFFFFFFFF : 0x7FFFFFFF;
              if (timeTag < fPreviousTime[iBrd][iCh]) {
                fTimeOffset[iBrd][iCh] += (TSMask + 1);
              }
              fPreviousTime[iBrd][iCh] = timeTag;
              uint64_t tdc = (timeTag + fTimeOffset[iBrd][iCh]) * fWDcfg.Tsampl;
              data->TimeStamp = tdc;
            }

            data->FineTS = (1000 * data->TimeStamp);
            // if (fFlagFineTS) {
            //   // For safety and to kill the rounding error, cleary using double
            //   double posZC = uint16_t((data->Extras >> 16) & 0xFFFF);
            //   double negZC = uint16_t(data->Extras & 0xFFFF);
            //   double thrZC = 8192;  // (1 << 13). (1 << 14) is maximum of ADC
            //   if (fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PSD ||
            //       fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PHA)
            //     thrZC += fWDcfg.TrgThreshold[iBrd][iCh];

            //   if ((negZC <= thrZC) && (posZC >= thrZC)) {
            //     double dt =
            //         (1 + fWDcfg.CFDinterp[iBrd][iCh] * 2) * fWDcfg.Tsampl;
            //     data->FineTS =
            //         ((dt * 1000. * (thrZC - negZC) / (posZC - negZC)) + 0.5);
            //   }
            // } else if (fFlagHWFineTS) {
            //   double fineTS = data->Extras & 0b1111111111;  // 10 bits
            //   data->FineTS = fWDcfg.Tsampl * 1000. * fineTS / (1024. - 1.);
            //   // data->FineTS = fWDcfg.Tsampl * fineTS;
            //   // data->FineTS = fineTS;
            // }
            // data->FineTS = data->FineTS + (1000 * data->TimeStamp);

            // if (fFlagTrgCounter[iBrd][iCh]) {
            //   // use fine ts as lost trigger counter;
            //   double lostTrg = uint16_t((data->Extras >> 16) & 0xFFFF);
            //   lostTrg += fLostTrgCounterOffset[iBrd][iCh] * 0xFFFF;
            //   if (fLostTrgCounter[iBrd][iCh] > lostTrg) {
            //     lostTrg += 0xFFFF;
            //     fLostTrgCounterOffset[iBrd][iCh]++;
            //   }
            //   fLostTrgCounter[iBrd][iCh] = lostTrg;
            //   data->FineTS = lostTrg;
            // }

            if (data->RecordLength > 0) {
              constexpr auto eleSizeShort = sizeof(data->Trace1[0]);
              memcpy(&(data->Trace1[0]), fpQDCWaveform[iBrd]->Trace1,
                     fpQDCWaveform[iBrd]->Ns * eleSizeShort);
              memcpy(&(data->Trace2[0]), fpQDCWaveform[iBrd]->Trace2,
                     fpQDCWaveform[iBrd]->Ns * eleSizeShort);

              constexpr auto eleSizeChar = sizeof(data->DTrace1[0]);
              memcpy(&(data->DTrace1[0]), fpQDCWaveform[iBrd]->DTrace1,
                     fpQDCWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace2[0]), fpQDCWaveform[iBrd]->DTrace2,
                     fpQDCWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace3[0]), fpQDCWaveform[iBrd]->DTrace3,
                     fpQDCWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace4[0]), fpQDCWaveform[iBrd]->DTrace4,
                     fpQDCWaveform[iBrd]->Ns * eleSizeChar);
            }

            fMutex.lock();
            fDataVec->push_back(data);
            fMutex.unlock();
          }
        }
      }
    }
  }
}

void TQDC::UseFineTS()
{
  // This is for the x725 and x730 series.
  // For other models, check the registers address.

  // When we used Extra data as the fine TS.  Digitizer returned
  // comb (rounding error effect) shaped distribution.
  // Using zero crossing information and calculting by program is better.
  // This is also the reason, extended time stamp is not used, and this
  // class calcultes the extended time stamp in ReadEvents().

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      // Using extra as fine TS
      // RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b010,
      //                 fWDcfg);
      // Using extra as zero crossing information
      // RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b101,
      //                 fWDcfg);
    }
  }

  SetTraces();

  fFlagHWFineTS = false;
  fFlagFineTS = true;
}

void TQDC::UseHWFineTS()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      // Using extra as fine TS
      // RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b010,
      //                 fWDcfg);
      // Using extra as zero crossing information
      // RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b101,
      //                 fWDcfg);
    }
  }

  SetTraces();

  fFlagFineTS = false;
  fFlagHWFineTS = true;
}

void TQDC::SetTraces()
{
  // Use DPP library instead of write registers
}

void TQDC::UseTrgCounter(const int mod, const int ch)
{
  // for (auto iMod = 0; iMod < MAX_NBRD; iMod++) {
  //   for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
  //     if ((mod == -1 || mod == iMod) && (ch == -1 || ch == iCh)) {
  //       fFlagTrgCounter[iMod][iCh] = true;
  //       RegisterSetBits(fHandler[iMod], 0x1084 + (iCh << 8), 8, 10, 0b100,
  //                       fWDcfg);
  //     }
  //   }
  // }
}

void TQDC::SetThreshold()
{
  // if (fDigitizerModel == 730 || fDigitizerModel == 725) {
  //   for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
  //     for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
  //       auto errCode =
  //           CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x1060 + (iCh << 8),
  //                                   fWDcfg.TrgThreshold[iBrd][iCh]);
  //       PrintError(errCode, "SetThreshold");
  //     }
  //   }
  // }
}

void TQDC::SetTrginVETO()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    // Probably 0x8000, 0x8004, bit 20 21 and following two lines???
    // CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x810C, 0x80000000);
    // RegisterSetBits(fHandler[iBrd], 0x811C, 10, 11, 3, fWDcfg);

    // // for (auto iPair = 0; iPair < 8; iPair++)
    // //   CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8180 + iPair * 4, 0);

    // RegisterSetBits(fHandler[iBrd], 0x80A0, 4, 5, 1, fWDcfg);

    // for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
    //   RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 2, 2, 1, fWDcfg);
    //   RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 6, 6, 1, fWDcfg);
    //   RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 14, 15, 1, fWDcfg);
    //   RegisterSetBits(fHandler[iBrd], 0x10D4 + (iCh << 8), 0, 15, 0, fWDcfg);
    // }

    // RegisterSetBits(fHandler[iBrd], 0x8080, 18, 19, 0b11, fWDcfg);
  }
}
void TQDC::SetTrginGate() {}
