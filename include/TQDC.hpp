#ifndef TQDC_hpp
#define TQDC_hpp 1

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

class TQDC : public TDigiTes
{
 public:
  TQDC();
  virtual ~TQDC();

  void InitDigitizers() override;

  // Memory
  void AllocateMemory() override;
  void FreeMemory() override;

  void UseFineTS() override;  // For fine TS
  void UseHWFineTS() override;
  void UseTrgCounter(const int mod, const int ch) override;

  void SetThreshold() override;

 private:
  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_QDC_Event_t **fppQDCEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_QDC_Waveforms_t *fpQDCWaveform[MAX_NBRD];  // waveforms buffer

  void ReadRawData() override;
  void DecodeRawData() override;
};

#endif
