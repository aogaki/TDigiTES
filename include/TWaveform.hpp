#ifndef TWaveform_hpp
#define TWaveform_hpp 1

#include <string>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TreeData.h"
#include "digiTES.h"

class TWaveform : public TDigiTes
{
 public:
  TWaveform();
  virtual ~TWaveform();

  // Memory
  void AllocateMemory() override;
  void FreeMemory() override;

  void DisableSelfTrigger();

  void UseFineTS(){};
  void UseHWFineTS(){};
  void UseTrgCounter(const int mod = -1, const int ch = -1){};

  void SetThreshold();

  void SetTraces() override;
  void ReadSmallData() override;

  void SetTrginGate() override;  // Test
  void SetTrginVETO() override;  // Test
  
 private:
  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                 // readout buffer
  CAEN_DGTZ_UINT16_EVENT_t *fpEventStd[MAX_NBRD];  // events buffer

  // time stamp
  uint64_t fTimeOffset;
  uint64_t fPreviousTime;

  void ReadRawData() override;
  void DecodeRawData() override;
};

#endif
