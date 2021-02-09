#ifndef TDigiTes_hpp
#define TDigiTes_hpp 1

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TPSDData.hpp"
#include "digiTES.h"

enum class FirmWareCode {
  // I need only theese
  // Its are not same as CAENDigitizerType.h
  DPP_PSD,
  DPP_PHA,
  DPP_ChInt,  // DPP_CI is already used by digiTes
  STD,
};

class TDigiTes
{
 public:
  TDigiTes();
  virtual ~TDigiTes();

  void LoadParameters(std::string fileName = "digiTES_Config.txt");
  const Config_t GetParameters() { return fWDcfg; };

  void OpenDigitizers();
  void CloseDigitizers();

  void InitDigitizers();

  void Start();
  void Stop();

  void SendSWTrigger();

  // Memory
  virtual void AllocateMemory() = 0;
  virtual void FreeMemory() = 0;

  virtual void ReadEvents() = 0;

  // void *GetData();

  virtual void UseFineTS() = 0;  // For fine TS

 protected:
  Config_t fWDcfg;     // acquisition parameters and user settings
  SysVars_t fSysVars;  // system variables
  int fHandler[MAX_NBRD];
  uint32_t fChMask[MAX_NBRD];

  void GetBoardInfo();
  int fTSample[MAX_NBRD];  // We need only this time sample information
  int fDigitizerModel;
  FirmWareCode fFirmware;
  uint32_t fNChs[MAX_NBRD];
  int fNBits;  // ADC, Waveform resolution

  // For Fine TS
  bool fFlagFineTS;
  uint64_t fPreviousTime[MAX_NBRD][MAX_NCH];
  uint64_t fTimeOffset[MAX_NBRD][MAX_NCH];

  void PrintError(const CAEN_DGTZ_ErrorCode &err,
                  const std::string &funcName = "") const;
};

#endif
