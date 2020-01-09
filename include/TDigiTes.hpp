#ifndef TDigiTes_hpp
#define TDigiTes_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
#include "Console.h"
#include "ParamParser.h"
#include "TPSDData.hpp"
#include "digiTES.h"

class TDigiTes
{
 public:
  TDigiTes();
  ~TDigiTes();

  void LoadParameters();

  void InitDigitizers();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void Start();
  void Stop();

  void ReadEvents();
  std::vector<TPSDData_t> *GetData() { return fDataVec; }

 private:
  Config_t fWDcfg;     // acquisition parameters and user settings
  SysVars_t fSysVars;  // system variables
  int fHandler[MAX_NBRD];

  std::vector<TPSDData_t> *fDataVec;

  // Memory
  char *fpReadoutBuffer;                         // readout buffer
  CAEN_DGTZ_DPP_PSD_Event_t **fppPSDEvents;      // events buffer
  CAEN_DGTZ_DPP_PSD_Waveforms_t *fpPSDWaveform;  // waveforms buffer

  void PrintError(const CAEN_DGTZ_ErrorCode &err, const std::string &funcName);
};

#endif
