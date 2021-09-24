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

  // In the ReadEvents, all elements of fDataVec are deleted.
  // After calling GetData, you have to copy or use the data before deleteing.
  void ReadEvents();
  std::vector<PHAData_t *> *GetData() { return fDataVec; }

  void UseFineTS();

  void SetThreshold();
  void SetTrapezoidPar();

 private:
  std::vector<PHAData_t *> *fDataVec;

  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_PHA_Event_t **fppPHAEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_PHA_Waveforms_t *fpPHAWaveform[MAX_NBRD];  // waveforms buffer

  // For time TimeStamp
  // uint64_t fPreviousTime[MAX_NBRD][MAX_NCH];
  // uint64_t fTimeOffset[MAX_NBRD][MAX_NCH];
};

#endif
