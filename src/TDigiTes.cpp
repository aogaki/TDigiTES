#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "TDigiTes.hpp"

TDigiTes::TDigiTes(){};

TDigiTes::~TDigiTes(){};

void TDigiTes::LoadParameters(std::string fileName)
{
  LoadSysVars(fSysVars);

  // auto configFileName = "digiTES_Config.txt";
  std::cout << "Open the input file " << fileName << std::endl;
  FILE *f_ini = fopen(fileName.c_str(), "r");

  if (f_ini == NULL) {
    std::cerr << "ERROR: Can't open configuration file " << fileName
              << std::endl;
    exit(1);
  }
  ParseConfigFile(f_ini, &fWDcfg, fSysVars);

  fclose(f_ini);

  // for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) fHandler[iBrd] = handle[iBrd];
}

void TDigiTes::OpenDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = CAEN_DGTZ_OpenDigitizer((CAEN_DGTZ_ConnectionType)fWDcfg.LinkType[b],
                                  fWDcfg.LinkNum[b], fWDcfg.ConetNode[b],
                                  fWDcfg.BaseAddress[b], &fHandler[b]);
    std::cout << fHandler[b] << "\t" << handle[b] << std::endl;
    handle[b] = fHandler[b];
    char cstr[500];
    if (ReadBoardInfo(b, cstr, fWDcfg) < 0) {
      std::cout << "ReadBoardInfo failed" << std::endl;
    }
    for (auto i = fWDcfg.NumAcqCh; i < MAX_NCH; i++)
      fWDcfg.EnableInput[b][i] = 0;

    SetTraceNames(fWDcfg);
  }
}

void TDigiTes::CloseDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = CAEN_DGTZ_CloseDigitizer(fHandler[b]);
  }
}

void TDigiTes::InitDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer(b, false, fWDcfg, fSysVars);
    if (ret) {
      std::cout << "ERROR: Failed to program the digitizer" << std::endl;
      exit(1);
    }
    SetVirtualProbes(b, fWDcfg);
  }
}

void TDigiTes::Start() { StartAcquisition(fWDcfg); }

void TDigiTes::Stop() { StopAcquisition(fWDcfg); }

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
