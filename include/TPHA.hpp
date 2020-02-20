#ifndef TPHA_hpp
#define TPHA_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TPHAData.hpp"
#include "digiTES.h"

class TPHA : public TDigiTes
{
 public:
  TPHA();
  virtual ~TPHA();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void ReadEvents();
  std::vector<PHAData_t *> *GetData() { return fDataVec; }

 private:
  std::vector<PHAData_t *> *fDataVec;

  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_PHA_Event_t **fppPHAEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_PHA_Waveforms_t *fpPHAWaveform[MAX_NBRD];  // waveforms buffer
};

#endif
