#ifndef TPHA_hpp
#define TPHA_hpp 1

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "BoardUtils.h"
#include "Configure.h"
// #include "Console.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TreeData.h"
#include "digiTES.h"

class TPHA : public TDigiTes
{
 public:
  TPHA();
  virtual ~TPHA();

  // Memory
  void AllocateMemory() override;
  void FreeMemory() override;

  void UseFineTS() override;
  void UseHWFineTS() override;
  void UseTrgCounter(const int mod, const int ch) override;

  void SetTrginVETO(uint32_t gateWidth = 0);  // Test
  void SetTrginGate(uint32_t gateWidth = 0);  // Test

  void SetThreshold() override;
  void SetTrapezoidPar();
  void SetPHAPar();  // Both threshold and trapezoid, and more

  void ReadSmallData() override;

 private:
  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_PHA_Event_t **fppPHAEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_PHA_Waveforms_t *fpPHAWaveform[MAX_NBRD];  // waveforms buffer

  void ReadRawData() override;
  void DecodeRawData() override;
};

#endif
