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
  }

  fDataVec = new std::vector<TPSDData_t *>;
};

TPSD::~TPSD()
{
  FreeMemory();
  for (auto &&ele : *fDataVec) delete ele;
  delete fDataVec;
};

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
  CAEN_DGTZ_ErrorCode err;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      PrintError(err, "FreeReadoutBuffer");
      fpReadoutBuffer[iBrd] = nullptr;
    }
    if (fppPSDEvents[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[iBrd], (void **)(fppPSDEvents[iBrd]));
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

    for (uint iCh = 0; iCh < 8; iCh++) {
      for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
        err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                           &(fppPSDEvents[iBrd][iCh][iEve]),
                                           fpPSDWaveform[iBrd]);
        PrintError(err, "DecodeDPPWaveforms");

        // For Extended time stamp
        auto tdc =
            fppPSDEvents[iBrd][iCh][iEve].TimeTag +
            ((uint64_t)((fppPSDEvents[iBrd][iCh][iEve].Extras >> 16) & 0xFFFF)
             << 31);

        auto data = new TPSDData(fpPSDWaveform[iBrd]->Ns);
        data->ModNumber = iBrd;
        data->ChNumber = iCh;
        data->TimeStamp = tdc;
        data->ChargeLong = fppPSDEvents[iBrd][iCh][iEve].ChargeLong;
        data->ChargeShort = fppPSDEvents[iBrd][iCh][iEve].ChargeShort;
        data->RecordLength = fpPSDWaveform[iBrd]->Ns;

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
