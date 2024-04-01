#include "TWaveform.hpp"

#include <CAENDigitizer.h>

#include <cstring>
#include <iostream>
#include <string>

TWaveform::TWaveform() : fTimeOffset(0), fPreviousTime(0)
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpEventStd[iBrd] = nullptr;
  }
}

TWaveform::~TWaveform() { FreeMemory(); }

void TWaveform::AllocateMemory()
{
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    // HACK: workaround to prevent memory allocation bug in the library:
    // allocate for all channels
    uint32_t EnableMask;
    CAEN_DGTZ_GetChannelEnableMask(fHandler[iBrd], &EnableMask);
    CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8120, 0xFFFF);
    err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[iBrd],
                                        &(fpReadoutBuffer[iBrd]), &size);
    PrintError(err, "MallocReadoutBuffer");
    CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8120, EnableMask);
    CAEN_DGTZ_WriteRegister(fHandler[iBrd], 0x8120, EnableMask);
  }
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_AllocateEvent(fHandler[iBrd], (void **)&fpEventStd[iBrd]);
    PrintError(err, "AllocateEvent");
  }
}

void TWaveform::FreeMemory()
{
  CAEN_DGTZ_ErrorCode err;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      fpReadoutBuffer[iBrd] = nullptr;
      PrintError(err, "FreeReadoutBuffer");
      err = CAEN_DGTZ_FreeEvent(fHandler[iBrd], (void **)&fpEventStd[iBrd]);
      PrintError(err, "FreeEvent");
    }
  }
};

void TWaveform::ReadRawData()
{
  RawData_t rawData(new std::vector<std::vector<char>>);

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    uint32_t bufferSize;
    auto err = CAEN_DGTZ_ReadData(fHandler[iBrd],
                                  CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                                  fpReadoutBuffer[iBrd], &bufferSize);
    PrintError(err, "ReadData");

    std::vector<char> v(bufferSize);
    if (bufferSize > 0) memcpy(&v[0], fpReadoutBuffer[iBrd], bufferSize);
    rawData->push_back(v);
  }

  fMutex.lock();
  fRawDataQue.push_back(rawData);
  fMutex.unlock();
}

void TWaveform::DecodeRawData()
{
  fMutex.lock();
  const auto nRawData = fRawDataQue.size();
  fMutex.unlock();

  if (nRawData > 0) {
    fMutex.lock();
    auto rawData = fRawDataQue.front();
    fRawDataQue.pop_front();
    fMutex.unlock();
    for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
      uint32_t bufferSize = rawData->at(iBrd).size();
      if (bufferSize == 0)
        continue;  // in the case of 0, GetDPPEvents makes crush

      uint32_t nEvents;
      auto err = CAEN_DGTZ_GetNumEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                        bufferSize, &nEvents);
      PrintError(err, "GetNumEvents");

      for (auto iEve = 0; iEve < nEvents; iEve++) {
        CAEN_DGTZ_EventInfo_t eventInfo;
        char *pEventPtr;
        err = CAEN_DGTZ_GetEventInfo(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                     bufferSize, iEve, &eventInfo, &pEventPtr);
        PrintError(err, "GetEventInfo");
        // std::cout << "Event number:\t" << iEve << '\n'
        //           << "Event size:\t" << eventInfo.EventSize << '\n'
        //           << "Board ID:\t" << eventInfo.BoardId << '\n'
        //           << "Pattern:\t" << eventInfo.Pattern << '\n'
        //           << "Ch mask:\t" << eventInfo.ChannelMask << '\n'
        //           << "Event counter:\t" << eventInfo.EventCounter << '\n'
        //           << "Trigger time tag:\t" << eventInfo.TriggerTimeTag
        //           << std::endl;

        err = CAEN_DGTZ_DecodeEvent(fHandler[iBrd], pEventPtr,
                                    (void **)&fpEventStd[iBrd]);
        PrintError(err, "DecodeEvent");

        uint64_t timeStamp =
            (eventInfo.TriggerTimeTag + fTimeOffset) * fTSample[iBrd];
        if (timeStamp < fPreviousTime) {
          constexpr uint32_t maxTime = 0xFFFFFFFF / 2;  // Check manual
          timeStamp += maxTime * fTSample[iBrd];
          fTimeOffset += maxTime;
        }
        fPreviousTime = timeStamp;

        for (uint32_t iCh = 0; iCh < fNChs[iBrd]; iCh++) {
          if (!((fChMask[iBrd] >> iCh) & 0x1)) continue;

          const uint32_t waveformSize = fpEventStd[iBrd]->ChSize[iCh];
          auto data = std::make_shared<TreeData_t>(waveformSize);
          data->Mod = iBrd + fStartMod;
          data->Ch = iCh;
          data->TimeStamp = timeStamp;
          data->FineTS = 1000. * timeStamp;
          data->ChargeLong = 0;
          data->ChargeShort = 0;
          data->RecordLength = waveformSize;
          constexpr auto eleSizeShort =
              sizeof(*fpEventStd[iBrd]->DataChannel[iCh]);
          std::memcpy(&(data->Trace1[0]), fpEventStd[iBrd]->DataChannel[iCh],
                      waveformSize * eleSizeShort);

          fMutex.lock();
          fDataVec->push_back(data);
          fMutex.unlock();
        }
      }
    }
  }
}

void TWaveform::DisableSelfTrigger()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (uint32_t iCh = 0; iCh < fNChs[iBrd]; iCh++) {
      CAEN_DGTZ_SetChannelSelfTrigger(fHandler[iBrd],
                                      CAEN_DGTZ_TRGMODE_DISABLED, (1 << iCh));
    }
  }
}

void TWaveform::SetThreshold()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    for (auto iCh = 0; iCh < fWDcfg.NumPhyCh; iCh++) {
      auto errCode = CAEN_DGTZ_SetChannelTriggerThreshold(
          fHandler[iBrd], iCh, fWDcfg.TrgThreshold[iBrd][iCh]);
      PrintError(errCode, "SetThreshold");
    }
  }
}

void TWaveform::SetTraces()
{
  // Just for virtual functions
}

void TWaveform::ReadSmallData()
{
  // Just for virtual functions
}

void TWaveform::SetTrginGate() {};  // Test
void TWaveform::SetTrginVETO() {};  // Test
  
