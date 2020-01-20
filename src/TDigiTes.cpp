#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TDigiTes.hpp"

TDigiTes::TDigiTes()
    : fpReadoutBuffer(nullptr), fppPSDEvents(nullptr), fpPSDWaveform(nullptr)
{
  fDataVec = new std::vector<TPSDData_t>;
};

TDigiTes::~TDigiTes()
{
  FreeMemory();
  delete fDataVec;
};

void TDigiTes::LoadParameters()
{
  LoadSysVars(fSysVars);

  // auto configFileName = "digiTES_Config.txt";
  auto configFileName = "/home/aogaki/DAQ/test/digiTES_Config.txt ";
  std::cout << "Open the input file " << configFileName << std::endl;
  FILE *f_ini = fopen(configFileName, "r");

  if (f_ini == NULL) {
    std::cerr << "ERROR: Can't open configuration file " << configFileName
              << std::endl;
    exit(1);
  }
  ParseConfigFile(f_ini, &fWDcfg, fSysVars);
  fclose(f_ini);

  // for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) fHandler[iBrd] = handle[iBrd];
}

void TDigiTes::InitDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = CAEN_DGTZ_OpenDigitizer((CAEN_DGTZ_ConnectionType)fWDcfg.LinkType[b],
                                  fWDcfg.LinkNum[b], fWDcfg.ConetNode[b],
                                  fWDcfg.BaseAddress[b], &fHandler[b]);
    char cstr[500];
    if (ReadBoardInfo(b, cstr, fWDcfg) < 0) {
      std::cout << "ReadBoardInfo failed" << std::endl;
    }
    for (b = 0; b < fWDcfg.NumBrd; b++)
      for (auto i = fWDcfg.NumAcqCh; i < MAX_NCH; i++)
        fWDcfg.EnableInput[b][i] = 0;

    SetTraceNames(fWDcfg);
  }

  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer(b, false, fWDcfg, fSysVars);
    if (ret) {
      std::cout << "ERROR: Failed to program the digitizer" << std::endl;
      exit(1);
    }
    SetVirtualProbes(b, fWDcfg);
  }
}

void TDigiTes::AllocateMemory()
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

void TDigiTes::FreeMemory()
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

void TDigiTes::Start() { StartAcquisition(fWDcfg); }

void TDigiTes::Stop() { StopAcquisition(fWDcfg); }

void TDigiTes::ReadEvents()
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

void TDigiTes::PrintError(const CAEN_DGTZ_ErrorCode &err,
                          const std::string &funcName)
{
  // if (true) {  // 0 is success
  if (err < 0) {  // 0 is success
    std::cout << "In " << funcName << ", error code = " << err << std::endl;
    // CAEN_DGTZ_CloseDigitizer(fHandler);
    // throw err;
  }
}
