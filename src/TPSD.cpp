#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TPSD.hpp"

TPSD::TPSD()
    : fpReadoutBuffer(nullptr), fppPSDEvents(nullptr), fpPSDWaveform(nullptr)
{
  fDataVec = new std::vector<TPSDData_t>;
};

TPSD::~TPSD()
{
  FreeMemory();
  delete fDataVec;
};

void TPSD::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[0], &fpReadoutBuffer, &size);
  PrintError(err, "MallocReadoutBuffer");

  // CAEN_DGTZ_DPP_PSD_Event_t *events[fNChs];
  fppPSDEvents = new CAEN_DGTZ_DPP_PSD_Event_t *[8];
  err = CAEN_DGTZ_MallocDPPEvents(fHandler[0], (void **)fppPSDEvents, &size);
  PrintError(err, "MallocDPPEvents");

  err =
      CAEN_DGTZ_MallocDPPWaveforms(fHandler[0], (void **)&fpPSDWaveform, &size);
  PrintError(err, "MallocDPPWaveforms");
}

void TPSD::FreeMemory()
{
  CAEN_DGTZ_ErrorCode err;
  if (fpReadoutBuffer != nullptr) {
    err = CAEN_DGTZ_FreeReadoutBuffer(&fpReadoutBuffer);
    PrintError(err, "FreeReadoutBuffer");
    fpReadoutBuffer = nullptr;
  }
  if (fppPSDEvents != nullptr) {
    err = CAEN_DGTZ_FreeDPPEvents(fHandler[0], (void **)fppPSDEvents);
    PrintError(err, "FreeDPPEvents");
    fppPSDEvents = nullptr;
  }
  if (fpPSDWaveform != nullptr) {
    err = CAEN_DGTZ_FreeDPPWaveforms(fHandler[0], fpPSDWaveform);
    PrintError(err, "FreeDPPWaveforms");
    fpPSDWaveform = nullptr;
  }
}

void TPSD::ReadEvents()
{
  fDataVec->clear();

  CAEN_DGTZ_ErrorCode err;

  uint32_t bufferSize;
  err = CAEN_DGTZ_ReadData(fHandler[0], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                           fpReadoutBuffer, &bufferSize);
  PrintError(err, "ReadData");
  if (bufferSize == 0) return;  // in the case of 0, GetDPPEvents makes crush

  uint32_t nEvents[8];
  err = CAEN_DGTZ_GetDPPEvents(fHandler[0], fpReadoutBuffer, bufferSize,
                               (void **)fppPSDEvents, nEvents);
  PrintError(err, "GetDPPEvents");
  // for (auto i = 0; i < 8; i++) std::cout << nEvents[i] << " hit\t";
  // std::cout << std::endl;

  for (uint iCh = 0; iCh < 8; iCh++) {
    for (uint iEve = 0; iEve < nEvents[iCh]; iEve++) {
      err = CAEN_DGTZ_DecodeDPPWaveforms(fHandler[0], &fppPSDEvents[iCh][iEve],
                                         fpPSDWaveform);
      PrintError(err, "DecodeDPPWaveforms");

      // For Extended time stamp
      auto tdc =
          fppPSDEvents[iCh][iEve].TimeTag +
          ((uint64_t)((fppPSDEvents[iCh][iEve].Extras >> 16) & 0xFFFF) << 31);
      // if (fTSample > 0) tdc *= fTSample;
      // Fine time stamp
      // tdc = (fppPSDEvents[iCh][iEve].Extras & 0x3FF);

      TPSDData_t data;
      data.ModNumber = 0;
      data.ChNumber = iCh;
      data.TimeStamp = tdc;
      data.ChargeLong = fppPSDEvents[iCh][iEve].ChargeLong;
      data.ChargeShort = fppPSDEvents[iCh][iEve].ChargeShort;
      data.RecordLength = fpPSDWaveform->Ns;
      data.WaveForm = fpPSDWaveform->Trace1;

      fDataVec->push_back(data);
    }
  }
}