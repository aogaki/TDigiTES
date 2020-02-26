#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TPHA.hpp"

TPHA::TPHA()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpPHAWaveform[iBrd] = nullptr;
    fppPHAEvents[iBrd] = nullptr;
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
      err = CAEN_DGTZ_FreeDPPEvents(fHandler[0], (void **)(fppPHAEvents[iBrd]));
      PrintError(err, "FreeDPPEvents");
      fppPHAEvents[iBrd] = nullptr;
    }
    if (fpPHAWaveform[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[0], fpPHAWaveform[iBrd]);
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
    if (bufferSize == 0) return;  // in the case of 0, GetDPPEvents makes crush

    uint32_t nEvents[MAX_NCH];
    err = CAEN_DGTZ_GetDPPEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                 bufferSize, (void **)(fppPHAEvents[iBrd]),
                                 nEvents);
    PrintError(err, "GetDPPEvents");

    for (uint iCh = 0; iCh < 8; iCh++) {
      for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
        err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[iBrd],
                                           &(fppPHAEvents[iBrd][iCh][iEve]),
                                           fpPHAWaveform[iBrd]);
        PrintError(err, "DecodeDPPWaveforms");

        // For Extended time stamp
        auto tdc =
            fppPHAEvents[iBrd][iCh][iEve].TimeTag +
            ((uint64_t)((fppPHAEvents[iBrd][iCh][iEve].Extras >> 16) & 0xFFFF)
             << 31);

        auto data = new PHAData(fpPHAWaveform[iBrd]->Ns);
        data->ModNumber = iBrd;
        data->ChNumber = iCh;
        data->TimeStamp = tdc;
        data->Energy = fppPHAEvents[iBrd][iCh][iEve].Energy;
        // data->RecordLength = fpPHAWaveform[iBrd]->Ns;

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
