#ifndef TPSD_hpp
#define TPSD_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TPSDData.hpp"
#include "digiTES.h"

class TPSD : public TDigiTes
{
 public:
  TPSD();
  virtual ~TPSD();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void ReadEvents();
  std::vector<PSDData_t *> *GetData() { return fDataVec; }

  void UseFineTS();  // For fine TS
  void UseHWFineTS();
  void UseTrgCounter(const int mod, const int ch);

  void SetThreshold();

 private:
  std::vector<PSDData_t *> *fDataVec;

  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_PSD_Event_t **fppPSDEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_PSD_Waveforms_t *fpPSDWaveform[MAX_NBRD];  // waveforms buffer
};

#endif
