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

// Following constants are for 1740
// Other models are not supported
constexpr uint32_t nGroups = 8;  // Have to check with digitizer information
constexpr uint32_t nChannels = 64;
constexpr uint32_t nChInGroup = 8;

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

  void ReadSmallData() override;

  void Test();

 private:
  // Memory
  char *fpReadoutBuffer[MAX_NBRD];                         // readout buffer
  CAEN_DGTZ_DPP_QDC_Event_t **fppQDCEvents[MAX_NBRD];      // events buffer
  CAEN_DGTZ_DPP_QDC_Waveforms_t *fpQDCWaveform[MAX_NBRD];  // waveforms buffer

  void ReadRawData() override;
  void DecodeRawData() override;

  bool fFlagExtTS;

  CAEN_DGTZ_DPP_QDC_Params_t fQDCParameters[MAX_NBRD];
  void LoadQDCParameters();

  void SetFrontPanel();
  void SetTRGIN();
  void SetTRGOUT();
  void SetSIN();
};

#endif
