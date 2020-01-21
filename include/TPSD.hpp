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
  ~TPSD();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void ReadEvents();
  std::vector<TPSDData_t> *GetData() { return fDataVec; }

 private:
  std::vector<TPSDData_t> *fDataVec;

  // Memory
  char *fpReadoutBuffer;                         // readout buffer
  CAEN_DGTZ_DPP_PSD_Event_t **fppPSDEvents;      // events buffer
  CAEN_DGTZ_DPP_PSD_Waveforms_t *fpPSDWaveform;  // waveforms buffer
};

#endif
