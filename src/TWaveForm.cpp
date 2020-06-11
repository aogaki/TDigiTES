#include <CAENDigitizer.h>

#include <cstring>
#include <iostream>
#include <string>

#include "TWaveForm.hpp"

TWaveForm::TWaveForm() : fpEventStd(nullptr), fTimeOffset(0), fPreviousTime(0) {
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    fpReadoutBuffer[iBrd] = nullptr;
  }

  fDataVec = new std::vector<WaveFormData_t *>;
}

TWaveForm::~TWaveForm() {
  FreeMemory();
  for (auto &&ele : *fDataVec) {
    delete ele;
  }
  delete fDataVec;
}

void TWaveForm::AllocateMemory() {
  CAEN_DGTZ_ErrorCode err;
  uint32_t size;

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    err = CAEN_DGTZ_MallocReadoutBuffer(fHandler[iBrd],
                                        &(fpReadoutBuffer[iBrd]), &size);
    PrintError(err, "MallocReadoutBuffer");
  }
}

void TWaveForm::FreeMemory() {
  CAEN_DGTZ_ErrorCode err;

  if (fpEventStd != nullptr)
    err = CAEN_DGTZ_FreeEvent(handle[0], (void **)&fpEventStd);
  fpEventStd = nullptr;
  PrintError(err, "FreeEvent");

  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    if (fpReadoutBuffer[iBrd] != nullptr) {
      err = CAEN_DGTZ_FreeReadoutBuffer(&(fpReadoutBuffer[iBrd]));
      PrintError(err, "FreeReadoutBuffer");
      fpReadoutBuffer[iBrd] = nullptr;
    }
  }
};

void TWaveForm::ReadEvents() {
  std::cout << "ReadEvents" << std::endl;
  for (auto &&ele : *fDataVec)
    delete ele;
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
                                  (void **)&fpEventStd);
      PrintError(err, "DecodeEvent");

      uint64_t timeStamp =
          (eventInfo.TriggerTimeTag + fTimeOffset) * fTSample[iBrd];
      if (timeStamp < fPreviousTime) {
        constexpr uint32_t maxTime = 0xFFFFFFFF / 2; // Check manual
        timeStamp += maxTime * fTSample[iBrd];
        fTimeOffset += maxTime;
      }
      fPreviousTime = timeStamp;

      std::cout << "Ch mask: " << fChMask[iBrd] << std::endl;

      for (uint32_t iCh = 0; iCh < 8; iCh++) {
        // if (!((fChMask[iBrd] >> iCh) & 0x1))
        //   continue;

        WaveFormData_t *dataEle = new TWaveFormData(fpEventStd->ChSize[iCh]);
        dataEle->ModNumber = iBrd;
        dataEle->ChNumber = iCh;
        dataEle->TimeStamp = timeStamp;
        dataEle->RecordLength = fpEventStd->ChSize[iCh];
        // dataEle->Trace1 = fpEventStd->DataChannel[iCh];
        std::memcpy(dataEle->Trace1, fpEventStd->DataChannel[iCh],
                    fpEventStd->ChSize[iCh]);

        fDataVec->push_back(dataEle);
      }
    }
  }
};
