#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TPSD.hpp"

TPSD::TPSD()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpPSDWaveform[iBrd] = nullptr;
    fppPSDEvents[iBrd] = nullptr;

    for (auto iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      fPreviousTime[iBrd][iCh] = 0;
      fTimeOffset[iBrd][iCh] = 0;
    }
  }

  fDataVec = new std::vector<PSDData_t *>;

  fFlagFineTS = false;
}

TPSD::~TPSD()
{
  FreeMemory();
  for (auto &&ele : *fDataVec) delete ele;
  delete fDataVec;
}

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

void TPSD::ReadEvents()
{
  for (auto &&ele : *fDataVec) delete ele;
  fDataVec->clear();

  CAEN_DGTZ_ErrorCode err;

  uint32_t bufferSize;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_ReadData(fHandler[iBrd],
                             CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                             fpReadoutBuffer[iBrd], &bufferSize);
    PrintError(err, "ReadData");
    if (bufferSize == 0) return;  // in the case of 0, GetDPPEvents makes crush

    uint32_t nEvents[MAX_NCH];
    err = CAEN_DGTZ_GetDPPEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                 bufferSize, (void **)(fppPSDEvents[iBrd]),
                                 nEvents);
    PrintError(err, "GetDPPEvents");

    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
        err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                           &(fppPSDEvents[iBrd][iCh][iEve]),
                                           fpPSDWaveform[iBrd]);
        PrintError(err, "DecodeDPPWaveforms");

        // For Extended time stamp
        // auto tdc =
        //     (fppPSDEvents[iBrd][iCh][iEve].TimeTag +
        //      ((uint64_t)((fppPSDEvents[iBrd][iCh][iEve].Extras >> 16) & 0xFFFF)
        //       << 31)) *
        //     fWDcfg.Tsampl;

        // Not use the Extended time stamp.
        // We want to use extra as zero crossing
        const auto TSMask =
            (fWDcfg.DppType == DPP_PSD_751) ? 0xFFFFFFFF : 0x7FFFFFFF;
        uint64_t timeTag = fppPSDEvents[iBrd][iCh][iEve].TimeTag;
        if (timeTag < fPreviousTime[iBrd][iCh]) {
          fTimeOffset[iBrd][iCh] += (TSMask + 1);
        }
        fPreviousTime[iBrd][iCh] = timeTag;
        auto tdc = (timeTag + fTimeOffset[iBrd][iCh]) * fWDcfg.Tsampl;

        // auto test = (timeTag + fTimeOffset[iBrd][iCh]) * fWDcfg.Tsampl;
        // std::cout << fppPSDEvents[iBrd][iCh][iEve].TimeTag << "\t" << tdc
        //           << "\t" << test << std::endl;
        // if (tdc != test) {
        //   exit(0);
        // }

        auto data = new PSDData(fpPSDWaveform[iBrd]->Ns);
        data->ModNumber = iBrd;
        data->ChNumber = iCh;
        data->TimeStamp = tdc;
        data->ChargeLong = fppPSDEvents[iBrd][iCh][iEve].ChargeLong;
        data->ChargeShort = fppPSDEvents[iBrd][iCh][iEve].ChargeShort;
        data->RecordLength = fpPSDWaveform[iBrd]->Ns;
        data->Extras = fppPSDEvents[iBrd][iCh][iEve].Extras;
        data->FineTS = 0;
        if (fFlagFineTS) {
          // For safety and to kill the rounding error, cleary using double
          double posZC = uint16_t((data->Extras >> 16) & 0xFFFF);
          double negZC = uint16_t(data->Extras & 0xFFFF);
          double thrZC = 8192;  // (1 << 13). (1 << 14) is maximum of ADC
          if (fWDcfg.DiscrMode[iBrd][iCh] == DISCR_MODE_LED_PSD)
            thrZC += fWDcfg.TrgThreshold[iBrd][iCh];

          if ((negZC <= thrZC) && (posZC >= thrZC)) {
            double dt = (1 + fWDcfg.CFDinterp[iBrd][iCh] * 2) * fWDcfg.Tsampl;
            data->FineTS =
                ((dt * 1000. * (thrZC - negZC) / (posZC - negZC)) + 0.5);
          }
        }

        constexpr auto eleSizeShort = sizeof(*data->Trace1);
        memcpy(data->Trace1, fpPSDWaveform[iBrd]->Trace1,
               fpPSDWaveform[iBrd]->Ns * eleSizeShort);
        memcpy(data->Trace2, fpPSDWaveform[iBrd]->Trace2,
               fpPSDWaveform[iBrd]->Ns * eleSizeShort);

        constexpr auto eleSizeChar = sizeof(*data->DTrace1);
        memcpy(data->DTrace1, fpPSDWaveform[iBrd]->DTrace1,
               fpPSDWaveform[iBrd]->Ns * eleSizeChar);
        memcpy(data->DTrace2, fpPSDWaveform[iBrd]->DTrace2,
               fpPSDWaveform[iBrd]->Ns * eleSizeChar);
        memcpy(data->DTrace3, fpPSDWaveform[iBrd]->DTrace3,
               fpPSDWaveform[iBrd]->Ns * eleSizeChar);
        memcpy(data->DTrace4, fpPSDWaveform[iBrd]->DTrace4,
               fpPSDWaveform[iBrd]->Ns * eleSizeChar);

        fDataVec->push_back(data);
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

    // Trace settings
    RegisterSetBits(fHandler[iBrd], 0x8000, 11, 11, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 12, 13, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 23, 25, 0b000, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 26, 28, 0b111, fWDcfg);
  }

  fFlagFineTS = true;
}
