#include <CAENDigitizer.h>

#include <cstring>
#include <iostream>
#include <string>

#include "TWaveform.hpp"

TWaveform::TWaveform() : fTimeOffset(0), fPreviousTime(0)
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
    fpEventStd[iBrd] = nullptr;
  }

  fDataVec = new std::vector<WaveformData_t *>;
}

TWaveform::~TWaveform()
{
  FreeMemory();
  for (auto &&ele : *fDataVec) {
    delete ele;
  }
  delete fDataVec;
}

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

void TWaveform::ReadEvents()
{
  for (auto &&ele : *fDataVec) delete ele;
  fDataVec->clear();

  CAEN_DGTZ_EventInfo_t eventInfo;
  char *pEventPtr;

  CAEN_DGTZ_ErrorCode err;
  uint32_t bufferSize;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_ReadData(fHandler[iBrd],
                             CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,
                             fpReadoutBuffer[iBrd], &bufferSize);
    PrintError(err, "ReadData");

    uint32_t nEvents;
    err = CAEN_DGTZ_GetNumEvents(fHandler[iBrd], fpReadoutBuffer[iBrd],
                                 bufferSize, &nEvents);
    PrintError(err, "GetNumEvents");

    for (auto iEve = 0; iEve < nEvents; iEve++) {
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

        // const uint16_t size = CAEN_DGTZ_GetRecordLength(fHandler[iBrd],
        // &tmp);
        const uint32_t size = fpEventStd[iBrd]->ChSize[iCh];
        WaveformData_t *dataEle = new TWaveformData(size);
        dataEle->ModNumber = iBrd;
        dataEle->ChNumber = iCh;
        dataEle->TimeStamp = timeStamp;
        dataEle->RecordLength = size;
        constexpr auto eleSizeShort =
            sizeof(*fpEventStd[iBrd]->DataChannel[iCh]);
        std::memcpy(dataEle->Trace1, fpEventStd[iBrd]->DataChannel[iCh],
                    size * eleSizeShort);

        fDataVec->push_back(dataEle);
      }
    }
  }
};

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
