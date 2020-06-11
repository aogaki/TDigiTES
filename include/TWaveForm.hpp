#ifndef TWaveForm_hpp
#define TWaveForm_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TWaveFormData.hpp"
#include "digiTES.h"

class TWaveForm : public TDigiTes {
public:
  TWaveForm();
  virtual ~TWaveForm();

  // Memory
  void AllocateMemory();
  void FreeMemory();

  void ReadEvents();
  std::vector<WaveFormData_t *> *GetData() { return fDataVec; }

private:
  std::vector<WaveFormData_t *> *fDataVec;

  // Memory
  char *fpReadoutBuffer[MAX_NBRD];      // readout buffer
  CAEN_DGTZ_UINT16_EVENT_t *fpEventStd; // events buffer

  // time stamp
  uint64_t fTimeOffset;
  uint64_t fPreviousTime;
};

#endif
