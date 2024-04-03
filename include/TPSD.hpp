#ifndef TPSD_hpp
#define TPSD_hpp 1

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

class TPSD : public TDigiTes
{
 public:
  TPSD();
  virtual ~TPSD();

  // Memory
  void AllocateMemory() override;
  void FreeMemory() override;

  void UseFineTS() override;  // For fine TS
  void UseHWFineTS() override;
  void UseTrgCounter(const int mod, const int ch) override;

  void SetThreshold() override;

  void ReadSmallData() override;

  void SetTraces() override;

  void SetTrginVETO() override;  // Test
  void SetTrginGate() override;  // Test

  void EnableLVDS() override;

 private:
  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_PSD_Event_t **fppPSDEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_PSD_Waveforms_t *fpPSDWaveform[MAX_NBRD];  // waveforms buffer

  void ReadRawData() override;
  void DecodeRawData() override;
};

#endif
