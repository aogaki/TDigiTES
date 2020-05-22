#ifndef TDigiTes_hpp
#define TDigiTes_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TPSDData.hpp"
#include "digiTES.h"

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

  // Memory
  virtual void AllocateMemory() = 0;
  virtual void FreeMemory() = 0;

  virtual void ReadEvents() = 0;

  // void *GetData();

 protected:
  Config_t fWDcfg;     // acquisition parameters and user settings
  SysVars_t fSysVars;  // system variables
  int fHandler[MAX_NBRD];

  void PrintError(const CAEN_DGTZ_ErrorCode &err,
                  const std::string &funcName = "") const;
};

#endif
