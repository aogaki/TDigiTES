#include "TQDC.hpp"

#include <CAENDigitizer.h>
#include <string.h>

#include <iostream>
#include <string>

TQDC::TQDC()
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    std::cout << iBrd << std::endl;
    fpReadoutBuffer[iBrd] = nullptr;
    fpQDCWaveform[iBrd] = nullptr;
    fppQDCEvents[iBrd] = nullptr;

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

TQDC::~TQDC() { FreeMemory(); }

void TQDC::InitDigitizers()
{
  // Simple copy of sample code for DPP_QDC
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_Reset(fHandler[iBrd]);
  }
  GetBoardInfo();
}

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
    if (fppQDCEvents[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[iBrd],
                                    (void **)(fppQDCEvents[iBrd]));
      PrintError(err, "FreeDPPEvents");
      fppQDCEvents[iBrd] = nullptr;
    }
    if (fpQDCWaveform[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[iBrd], fpQDCWaveform[iBrd]);
      PrintError(err, "FreeDPPWaveforms");
      fpQDCWaveform[iBrd] = nullptr;
    }
  }
}

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
        for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
          for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
            err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                               &(fppQDCEvents[iBrd][iCh][iEve]),
                                               fpQDCWaveform[iBrd]);
            PrintError(err, "DecodeDPPWaveforms");

            auto data = std::make_shared<TreeData_t>(fpQDCWaveform[iBrd]->Ns);
            data->Mod = iBrd;
            data->Ch = iCh;
            data->ChargeLong = fppQDCEvents[iBrd][iCh][iEve].Charge;
            data->ChargeShort = 0;
            data->RecordLength = fpQDCWaveform[iBrd]->Ns;
            data->Extras = fppQDCEvents[iBrd][iCh][iEve].Extras;

            uint64_t timeTag = fppQDCEvents[iBrd][iCh][iEve].TimeTag;
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
      RegisterSetBits(fHandler[iBrd], 0x1084 + (iCh << 8), 8, 10, 0b101,
                      fWDcfg);
    }

    // Trace settings
    RegisterSetBits(fHandler[iBrd], 0x8000, 11, 11, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 12, 13, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 23, 25, 0b000, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 26, 28, 0b111, fWDcfg);
  }

  fFlagHWFineTS = false;
  fFlagFineTS = true;
}

void TQDC::UseHWFineTS()
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

    // Trace settings
    RegisterSetBits(fHandler[iBrd], 0x8000, 11, 11, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 12, 13, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 23, 25, 0b000, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 26, 28, 0b111, fWDcfg);
  }

  fFlagFineTS = false;
  fFlagHWFineTS = true;
}

void TQDC::UseTrgCounter(const int mod, const int ch)
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

void TQDC::SetThreshold()
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
