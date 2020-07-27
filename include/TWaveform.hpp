#ifndef TWaveform_hpp
#define TWaveform_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TWaveformData.hpp"
#include "digiTES.h"

class TWaveform : public TDigiTes {
public:
  TWaveform();
  virtual ~TWaveform();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void ReadEvents();
  std::vector<WaveformData_t *> *GetData() { return fDataVec; }

  void DisableSelfTrigger();

private:
  std::vector<WaveformData_t *> *fDataVec;

  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                // readout buffer
  CAEN_DGTZ_UINT16_EVENT_t *fpEventStd[MAX_NBRD]; // events buffer

  // time stamp
  uint64_t fTimeOffset;
  uint64_t fPreviousTime;
};

#endif
