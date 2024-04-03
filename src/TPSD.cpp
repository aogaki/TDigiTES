#include "TPSD.hpp"

#include <CAENDigitizer.h>
#include <string.h>

#include <iostream>
#include <string>

TPSD::TPSD()
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpPSDWaveform[iBrd] = nullptr;
    fppPSDEvents[iBrd] = nullptr;

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
}

TPSD::~TPSD() { FreeMemory(); }

void TPSD::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[iBrd],
                                        &(fpReadoutBuffer[iBrd]), &size);
    PrintError(err, "MallocReadoutBuffer");

    // CAEN_DGTZ_DPP_PSD_Event_t *events[fNChs];
    fppPSDEvents[iBrd] = new CAEN_DGTZ_DPP_PSD_Event_t *[MAX_NCH];
    err = CAEN_DGTZ_MallocDPPEvents(fHandler[iBrd],
                                    (void **)(fppPSDEvents[iBrd]), &size);
    PrintError(err, "MallocDPPEvents");

    err = CAEN_DGTZ_MallocDPPWaveforms(fHandler[iBrd],
                                       (void **)&(fpPSDWaveform[iBrd]), &size);
    PrintError(err, "MallocDPPWaveforms");
  }
}

void TPSD::FreeMemory()
{
  // In digiTes, fppPSDEvents is not the array.
  // I have to check the double freeing memory
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      PrintError(err, "FreeReadoutBuffer");
      fpReadoutBuffer[iBrd] = nullptr;
    }
    if (fppPSDEvents[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[iBrd],
                                    (void **)(fppPSDEvents[iBrd]));
      PrintError(err, "FreeDPPEvents");
      fppPSDEvents[iBrd] = nullptr;
    }
    if (fpPSDWaveform[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[iBrd], fpPSDWaveform[iBrd]);
      PrintError(err, "FreeDPPWaveforms");
      fpPSDWaveform[iBrd] = nullptr;
    }
  }
}
void TPSD::ReadSmallData()
{
  fSmallDataVec = new std::vector<SmallData_t *>();
  fSmallDataVec->reserve(100000);

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    uint32_t bufferSize;
    auto err = CAEN_DGTZ_ReadData(fHandler[iBrd],
                                  CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                                  fpReadoutBuffer[iBrd], &bufferSize);
    PrintError(err, "ReadData");
    // in the case of 0, GetDPPEvents makes crush
    if (bufferSize == 0) continue;

    uint32_t nEvents[MAX_NCH];
    err = CAEN_DGTZ_GetDPPEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                 bufferSize, (void **)(fppPSDEvents[iBrd]),
                                 nEvents);
    PrintError(err, "GetDPPEvents");

    if (err == CAEN_DGTZ_Success) {
      for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
        for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
          err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                             &(fppPSDEvents[iBrd][iCh][iEve]),
                                             fpPSDWaveform[iBrd]);
          PrintError(err, "DecodeDPPWaveforms");

          auto data = new SmallData_t();
          data->Mod = iBrd;
          data->Ch = iCh;
          data->ChargeLong = fppPSDEvents[iBrd][iCh][iEve].ChargeLong;
          auto Extras = fppPSDEvents[iBrd][iCh][iEve].Extras;
          uint64_t TimeStamp = 0;

          uint64_t timeTag = fppPSDEvents[iBrd][iCh][iEve].TimeTag;
          if (fFlagHWFineTS) {
            uint64_t extTS = ((uint64_t)((Extras >> 16) & 0xFFFF) << 31);
            uint64_t tdc = (timeTag + extTS) * fWDcfg.Tsampl;
            TimeStamp = tdc;
          } else {
            const uint64_t TSMask =
                (fWDcfg.DppType == DPP_PSD_751) ? 0xFFFFFFFF : 0x7FFFFFFF;
            if (timeTag < fPreviousTime[iBrd][iCh]) {
              fTimeOffset[iBrd][iCh] += (TSMask + 1);
            }
            fPreviousTime[iBrd][iCh] = timeTag;
            uint64_t tdc = (timeTag + fTimeOffset[iBrd][iCh]) * fWDcfg.Tsampl;
            TimeStamp = tdc;
          }

          data->FineTS = 0.;
          if (fFlagFineTS) {
            // For safety and to kill the rounding error, cleary using double
            double posZC = uint16_t((Extras >> 16) & 0xFFFF);
            double negZC = uint16_t(Extras & 0xFFFF);
            double thrZC = 8192;  // (1 << 13). (1 << 14) is maximum of ADC
            if (fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PSD ||
                fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PHA)
              thrZC += fWDcfg.TrgThreshold[iBrd][iCh];

            if ((negZC <= thrZC) && (posZC >= thrZC)) {
              double dt = (1 + fWDcfg.CFDinterp[iBrd][iCh] * 2) * fWDcfg.Tsampl;
              data->FineTS =
                  ((dt * 1000. * (thrZC - negZC) / (posZC - negZC)) + 0.5);
            }
          } else if (fFlagHWFineTS) {
            double fineTS = Extras & 0b1111111111;  // 10 bits
            data->FineTS = fWDcfg.Tsampl * 1000. * fineTS / (1024. - 1.);
            // data->FineTS = fWDcfg.Tsampl * fineTS;
            // data->FineTS = fineTS;
          }
          data->FineTS = data->FineTS + (1000 * TimeStamp);

          if (fFlagTrgCounter[iBrd][iCh]) {
            // use fine ts as lost trigger counter;
            double lostTrg = uint16_t((Extras >> 16) & 0xFFFF);
            lostTrg += fLostTrgCounterOffset[iBrd][iCh] * 0xFFFF;
            if (fLostTrgCounter[iBrd][iCh] > lostTrg) {
              lostTrg += 0xFFFF;
              fLostTrgCounterOffset[iBrd][iCh]++;
            }
            fLostTrgCounter[iBrd][iCh] = lostTrg;
            data->FineTS = lostTrg;
          }

          fSmallDataVec->push_back(data);
        }
      }
    }
  }
}

void TPSD::ReadRawData()
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

void TPSD::DecodeRawData()
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
                                        (void **)(fppPSDEvents[iBrd]), nEvents);
      PrintError(err, "GetDPPEvents");

      if (err == CAEN_DGTZ_Success) {
        for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
          for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
            err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                               &(fppPSDEvents[iBrd][iCh][iEve]),
                                               fpPSDWaveform[iBrd]);
            PrintError(err, "DecodeDPPWaveforms");

            auto data = std::make_shared<TreeData_t>(fpPSDWaveform[iBrd]->Ns);
            data->Mod = iBrd + fStartMod;
            data->Ch = iCh;
            data->ChargeLong = fppPSDEvents[iBrd][iCh][iEve].ChargeLong;
            data->ChargeShort = fppPSDEvents[iBrd][iCh][iEve].ChargeShort;
            data->RecordLength = fpPSDWaveform[iBrd]->Ns;
            data->Extras = fppPSDEvents[iBrd][iCh][iEve].Extras;

            uint64_t timeTag = fppPSDEvents[iBrd][iCh][iEve].TimeTag;
            if (fFlagHWFineTS) {
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

            data->FineTS = 0.;
            if (fFlagFineTS) {
              // For safety and to kill the rounding error, cleary using double
              double posZC = uint16_t((data->Extras >> 16) & 0xFFFF);
              double negZC = uint16_t(data->Extras & 0xFFFF);
              double thrZC = 8192;  // (1 << 13). (1 << 14) is maximum of ADC
              if (fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PSD ||
                  fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PHA)
                thrZC += fWDcfg.TrgThreshold[iBrd][iCh];

              if ((negZC <= thrZC) && (posZC >= thrZC)) {
                double dt =
                    (1 + fWDcfg.CFDinterp[iBrd][iCh] * 2) * fWDcfg.Tsampl;
                data->FineTS =
                    ((dt * 1000. * (thrZC - negZC) / (posZC - negZC)) + 0.5);
              }
            } else if (fFlagHWFineTS) {
              double fineTS = data->Extras & 0b1111111111;  // 10 bits
              data->FineTS = fWDcfg.Tsampl * 1000. * fineTS / (1024. - 1.);
              // data->FineTS = fWDcfg.Tsampl * fineTS;
              // data->FineTS = fineTS;
            }
            data->FineTS = data->FineTS + (1000 * data->TimeStamp);

            if (fFlagTrgCounter[iBrd][iCh]) {
              // use fine ts as lost trigger counter;
              double lostTrg = uint16_t((data->Extras >> 16) & 0xFFFF);
              lostTrg += fLostTrgCounterOffset[iBrd][iCh] * 0xFFFF;
              if (fLostTrgCounter[iBrd][iCh] > lostTrg) {
                lostTrg += 0xFFFF;
                fLostTrgCounterOffset[iBrd][iCh]++;
              }
              fLostTrgCounter[iBrd][iCh] = lostTrg;
              data->FineTS = lostTrg;
            }

            if (data->RecordLength > 0) {
              constexpr auto eleSizeShort = sizeof(data->Trace1[0]);
              memcpy(&(data->Trace1[0]), fpPSDWaveform[iBrd]->Trace1,
                     fpPSDWaveform[iBrd]->Ns * eleSizeShort);
              memcpy(&(data->Trace2[0]), fpPSDWaveform[iBrd]->Trace2,
                     fpPSDWaveform[iBrd]->Ns * eleSizeShort);

              constexpr auto eleSizeChar = sizeof(data->DTrace1[0]);
              memcpy(&(data->DTrace1[0]), fpPSDWaveform[iBrd]->DTrace1,
                     fpPSDWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace2[0]), fpPSDWaveform[iBrd]->DTrace2,
                     fpPSDWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace3[0]), fpPSDWaveform[iBrd]->DTrace3,
                     fpPSDWaveform[iBrd]->Ns * eleSizeChar);
              memcpy(&(data->DTrace4[0]), fpPSDWaveform[iBrd]->DTrace4,
                     fpPSDWaveform[iBrd]->Ns * eleSizeChar);
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

void TPSD::UseFineTS()
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
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b101,
                      fWDcfg);
    }
  }

  fFlagHWFineTS = false;
  fFlagFineTS = true;
  SetTraces();
}

void TPSD::UseHWFineTS()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      // Using extra as fine TS
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b010,
                      fWDcfg);
      // Using extra as zero crossing information
      // RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b101,
      //                 fWDcfg);
    }
  }

  fFlagFineTS = false;
  fFlagHWFineTS = true;
  SetTraces();
}

void TPSD::SetTraces()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    RegisterSetBits(fHandler[iBrd], 0x8000, 11, 11, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 12, 13, 2, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 23, 25, 0b000, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 26, 28, 0b000, fWDcfg);
  }
}

void TPSD::UseTrgCounter(const int mod, const int ch)
{
  for (auto iMod = 0; iMod < MAX_NBRD; iMod++) {
    for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
      if ((mod == -1 || mod == iMod) && (ch == -1 || ch == iCh)) {
        fFlagTrgCounter[iMod][iCh] = true;
        RegisterSetBits(fHandler[iMod], 0x1084 + (iCh << 8), 8, 10, 0b100,
                        fWDcfg);
      }
    }
  }
}

void TPSD::SetThreshold()
{
  if (fDigitizerModel == 730 || fDigitizerModel == 725) {
    for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
      for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
        auto errCode =
            CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x1060 + (iCh << 8),
                                    fWDcfg.TrgThreshold[iBrd][iCh]);
        PrintError(errCode, "SetThreshold");
      }
    }
  }
}

void TPSD::SetTrginVETO()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x810C, 0x80000000);
    RegisterSetBits(fHandler[iBrd], 0x811C, 10, 11, 3, fWDcfg);

    for (auto iPair = 0; iPair < 8; iPair++)
      CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8180 + iPair * 4, 0);

    RegisterSetBits(fHandler[iBrd], 0x8084, 4, 5, 1, fWDcfg);

    for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 2, 2, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 6, 6, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 18, 19, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x10D4 + (iCh << 8), 0, 15, 0, fWDcfg);
    }

    RegisterSetBits(fHandler[iBrd], 0x8080, 18, 19, 0b11, fWDcfg);
  }
}

void TPSD::SetTrginGate()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x810C, 0x80000000);
    RegisterSetBits(fHandler[iBrd], 0x811C, 10, 11, 3, fWDcfg);

    for (auto iPair = 0; iPair < 8; iPair++)
      CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8180 + iPair * 4, 0);

    RegisterSetBits(fHandler[iBrd], 0x8084, 4, 5, 1, fWDcfg);

    for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 2, 2, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 6, 6, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 18, 19, 1, fWDcfg);
      RegisterSetBits(fHandler[iBrd], 0x10D4 + (iCh << 8), 0, 15, 0, fWDcfg);
    }

    RegisterSetBits(fHandler[iBrd], 0x8080, 18, 19, 0b01, fWDcfg);
  }
}

void TPSD::EnableLVDS()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    // Enable LVDS I/O Busy Enable
    RegisterSetBits(fHandler[iBrd], 0x8100, 8, 8, 1, fWDcfg);

    // Enable LVDS new feature
    RegisterSetBits(fHandler[iBrd], 0x811C, 8, 8, 1, fWDcfg);

    // Set all LVDS as output
    RegisterSetBits(fHandler[iBrd], 0x811C, 2, 2, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x811C, 3, 3, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x811C, 4, 4, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x811C, 5, 5, 1, fWDcfg);

    // Set output busy
    RegisterSetBits(fHandler[iBrd], 0x81A0, 0, 3, 0b0010, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x81A0, 4, 7, 0b0010, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x81A0, 8, 11, 0b0010, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x81A0, 12, 15, 0b0010, fWDcfg);
  }
}
