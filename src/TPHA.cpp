#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TPHA.hpp"

TPHA::TPHA()
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    std::cout << iBrd << std::endl;
    fpReadoutBuffer[iBrd] = nullptr;
    fpPHAWaveform[iBrd] = nullptr;
    fppPHAEvents[iBrd] = nullptr;

    for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
      fPreviousTime[iBrd][iCh] = 0;
      fTimeOffset[iBrd][iCh] = 0;
    }
  }

  fDataVec = new std::vector<PHAData_t *>;
};

TPHA::~TPHA()
{
  FreeMemory();
  for (auto &&ele : *fDataVec) delete ele;
  delete fDataVec;
};

void TPHA::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[iBrd],
                                        &(fpReadoutBuffer[iBrd]), &size);
    PrintError(err, "MallocReadoutBuffer");

    // CAEN_DGTZ_DPP_PHA_Event_t *events[fNChs];
    fppPHAEvents[iBrd] = new CAEN_DGTZ_DPP_PHA_Event_t *[MAX_NCH];
    err = CAEN_DGTZ_MallocDPPEvents(fHandler[iBrd],
                                    (void **)(fppPHAEvents[iBrd]), &size);
    PrintError(err, "MallocDPPEvents");

    err = CAEN_DGTZ_MallocDPPWaveforms(fHandler[iBrd],
                                       (void **)&(fpPHAWaveform[iBrd]), &size);
    PrintError(err, "MallocDPPWaveforms");
  }
}

void TPHA::FreeMemory()
{
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      PrintError(err, "FreeReadoutBuffer");
      fpReadoutBuffer[iBrd] = nullptr;
    }
    if (fppPHAEvents[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[iBrd],
                                    (void **)(fppPHAEvents[iBrd]));
      PrintError(err, "FreeDPPEvents");
      fppPHAEvents[iBrd] = nullptr;
    }
    if (fpPHAWaveform[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[iBrd], fpPHAWaveform[iBrd]);
      PrintError(err, "FreeDPPWaveforms");
      fpPHAWaveform[iBrd] = nullptr;
    }
  }
}

void TPHA::ReadEvents()
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
    if (bufferSize == 0)
      continue;  // in the case of 0, GetDPPEvents makes crush

    uint32_t nEvents[MAX_NCH];
    err = CAEN_DGTZ_GetDPPEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                 bufferSize, (void **)(fppPHAEvents[iBrd]),
                                 nEvents);
    PrintError(err, "GetDPPEvents");
    if (err == CAEN_DGTZ_Success) {
      for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
        for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
          err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                             &(fppPHAEvents[iBrd][iCh][iEve]),
                                             fpPHAWaveform[iBrd]);
          PrintError(err, "DecodeDPPWaveforms");

          // For Extended time stamp
          // For NOT x724
          // unsigned long test =
          //     ((fppPHAEvents[iBrd][iCh][iEve].TimeTag & 0x7FFFFFFF) +
          //      ((uint64_t)((fppPHAEvents[iBrd][iCh][iEve].Extras2 >> 16) & 0xFFFF)
          //       << 31)) *
          //     fWDcfg.Tsampl;

          // Extended timestamp without Extras2
          constexpr unsigned long TSMask = 0x7FFFFFFF;
          uint64_t timeTag = fppPHAEvents[iBrd][iCh][iEve].TimeTag;
          if (timeTag < fPreviousTime[iBrd][iCh]) {
            fTimeOffset[iBrd][iCh] += (TSMask + 1);
          }
          fPreviousTime[iBrd][iCh] = timeTag;
          unsigned long tdc =
              (timeTag + fTimeOffset[iBrd][iCh]) * fWDcfg.Tsampl;
          /*
          if(timeTag == 0){
            std::cout << fppPHAEvents[iBrd][iCh][iEve].TimeTag <<"\t"<< timeTag <<"\t"
                      <<  fTimeOffset[iBrd][iCh] <<"\t"<< fWDcfg.Tsampl
                      <<"\t"<< iBrd <<"\t"<< iCh <<"\t"
                      << fppPHAEvents[iBrd][iCh][iEve].Energy <<"\t"
                      << fWDcfg.EnableMask[iBrd] << std::endl;
          }
          */

          auto data = new PHAData(fpPHAWaveform[iBrd]->Ns);
          data->ModNumber = iBrd;
          data->ChNumber = iCh;
          data->TimeStamp = tdc;
          data->Extras = fppPHAEvents[iBrd][iCh][iEve].Extras2;
          data->FineTS = 0;
          if (fFlagFineTS) {
            double posZC = int16_t((data->Extras >> 16) & 0xFFFF);
            double negZC = int16_t(data->Extras & 0xFFFF);

            if ((negZC < 0) && (posZC >= 0)) {
              double dt = (1 + fWDcfg.CFDinterp[iBrd][iCh] * 2) * fWDcfg.Tsampl;
              // Which is better?  Using pos or neg
              // data->FineTS = ((dt * 1000. * posZC / (posZC - negZC)) + 0.5);
              data->FineTS =
                  ((dt * 1000. * (0 - negZC) / (posZC - negZC)) + 0.5);
            }
            // std::cout << negZC << "\t" << posZC << "\t" << data->FineTS
            //           << std::endl;
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

          data->Energy = fppPHAEvents[iBrd][iCh][iEve].Energy;
          data->RecordLength = fpPHAWaveform[iBrd]->Ns;

          constexpr auto eleSizeShort = sizeof(*data->Trace1);
          memcpy(data->Trace1, fpPHAWaveform[iBrd]->Trace1,
                 fpPHAWaveform[iBrd]->Ns * eleSizeShort);
          memcpy(data->Trace2, fpPHAWaveform[iBrd]->Trace2,
                 fpPHAWaveform[iBrd]->Ns * eleSizeShort);

          constexpr auto eleSizeChar = sizeof(*data->DTrace1);
          memcpy(data->DTrace1, fpPHAWaveform[iBrd]->DTrace1,
                 fpPHAWaveform[iBrd]->Ns * eleSizeChar);
          memcpy(data->DTrace2, fpPHAWaveform[iBrd]->DTrace2,
                 fpPHAWaveform[iBrd]->Ns * eleSizeChar);

          fDataVec->push_back(data);
        }
      }
    }
  }
}

void TPHA::UseFineTS()
{
  // This is for the x725 and x730 series.
  // For other models, check the registers address.

  // When we used Extra data as the fine TS.  Digitizer returned
  // comb (rounding error effect) shaped distribution.
  // Using zero crossing information and calculting by program is better.
  // This is also the reason, extended time stamp is not used, and this
  // class calcultes the extended time stamp in ReadEvents().

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    RegisterSetBits(fHandler[iBrd], 0x8000, 17, 17, 1, fWDcfg);
    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      // Using extra as fine TS
      // RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 8, 10, 0b010,
      //                 fWDcfg);
      // Using extra as zero crossing information
      RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 8, 10, 0b101,
                      fWDcfg);
    }

    // Trace settings
    RegisterSetBits(fHandler[iBrd], 0x8000, 11, 11, 1, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 12, 13, 0b00, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 14, 15, 0b01, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 20, 23, 0b0001, fWDcfg);
    RegisterSetBits(fHandler[iBrd], 0x8000, 26, 28, 0b000, fWDcfg);
  }

  fFlagFineTS = true;
}

void TPHA::UseHWFineTS()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (uint iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      // Using extra as fine TS
      RegisterSetBits(fHandler[iBrd], 0x10A0 + (iCh << 8), 8, 10, 0b010,
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

void TPHA::UseTrgCounter(const int mod, const int ch)
{
  for (auto iMod = 0; iMod < MAX_NBRD; iMod++) {
    for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
      if ((mod == -1 || mod == iMod) && (ch == -1 || ch == iCh)) {
        fFlagTrgCounter[iMod][iCh] = true;
        RegisterSetBits(fHandler[iMod], 0x10A0 + (iCh << 8), 8, 10, 0b100,
                        fWDcfg);
      }
    }
  }
}

void TPHA::SetTrapezoidPar()
{
  if (fFirmware == FirmWareCode::DPP_PHA &&
      (fDigitizerModel == 730 || fDigitizerModel == 725)) {
    for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
      for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
        auto timeUnit =
            (fWDcfg.Tsampl * 4) * (1 << fWDcfg.Decimation[iBrd][iCh]);
        int riseTime = fWDcfg.TrapRiseTime[iBrd][iCh] / timeUnit;
        int flatTop = fWDcfg.TrapFlatTop[iBrd][iCh] / timeUnit;
        int poleZero = fWDcfg.TrapPoleZero[iBrd][iCh] / timeUnit;

        auto errCode = CAEN_DGTZ_WriteRegister(fHandler[iBrd],
                                               0x105C + (iCh << 8), riseTime);
        PrintError(errCode, "SetRiseTime");
        errCode = CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x1060 + (iCh << 8),
                                          flatTop);
        PrintError(errCode, "SetFlatTop");
        errCode = CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x1068 + (iCh << 8),
                                          poleZero);
        PrintError(errCode, "SetPoleZero");
      }
    }
  }
}

void TPHA::SetThreshold()
{
  if (fDigitizerModel == 730 || fDigitizerModel == 725) {
    for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
      for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
        auto errCode =
            CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x106c + (iCh << 8),
                                    fWDcfg.TrgThreshold[iBrd][iCh]);
        PrintError(errCode, "SetThreshold");
      }
    }
  }
}

void TPHA::SetPHAPar()
{
  if (fDigitizerModel == 730 || fDigitizerModel == 725) {
    for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
      CAEN_DGTZ_DPP_PHA_Params_t par;

      for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
        par.M[iCh] = fWDcfg.TrapPoleZero[iBrd][iCh];
        par.m[iCh] = fWDcfg.TrapFlatTop[iBrd][iCh];
        par.k[iCh] = fWDcfg.TrapRiseTime[iBrd][iCh];
        par.ftd[iCh] = fWDcfg.PeakingTime[iBrd][iCh];
        par.a[iCh] = fWDcfg.TTFsmoothing[iBrd][iCh];
        par.b[iCh] = fWDcfg.TTFdelay[iBrd][iCh];  // Same as signal rise time
        par.thr[iCh] = fWDcfg.TrgThreshold[iBrd][iCh];
        par.nsbl[iCh] = fWDcfg.NsBaseline[iBrd][iCh];
        par.nspk[iCh] = fWDcfg.NSPeak[iBrd][iCh];
        par.pkho[iCh] = fWDcfg.PeakHoldOff[iBrd][iCh];
        // par.blho[iCh] = fWDcfg.BaseLineHoldOff[iBrd][iCh];
        par.blho[iCh] = 4;  // This was already removed from register setting?
        par.trgho[iCh] = fWDcfg.TrgHoldOff;
        // par.twwdt[iCh] = fWDcfg.ZeroCrossAccWindow[iBrd][iCh];
        par.twwdt[iCh] = 0;  // Rise time discrimination, 0 means disable
        par.trgwin[iCh] = fWDcfg.CoincWindow;
        // par.dgain[iCh] = fWDcfg.DigitalGain[iBrd][iCh];
        par.dgain[iCh] = 0;
        par.enf[iCh] = fWDcfg.EnergyFineGain[iBrd][iCh];
        par.decimation[iCh] = fWDcfg.Decimation[iBrd][iCh];
      }

      auto errCode = CAEN_DGTZ_SetDPPParameters(
          fHandler[iBrd], (fWDcfg.EnableMask[iBrd]) & ((1 << fNChs[iBrd]) - 1),
          &par);
      PrintError(errCode, "SetDPPParameters");
    }
  }
}
