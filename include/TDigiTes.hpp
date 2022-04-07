#ifndef TDigiTes_hpp
#define TDigiTes_hpp 1

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

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
#include "TreeData.h"
#include "digiTES.h"

enum class FirmWareCode {
  // I need only theese
  // Its are not same as CAENDigitizerType.h
  DPP_PSD,
  DPP_PHA,
  DPP_ChInt,  // DPP_CI is already used by digiTes
  STD,
};

class TDigiTes
{
 public:
  TDigiTes();
  virtual ~TDigiTes();

  void LoadParameters(std::string fileName = "digiTES_Config.txt");
  Config_t GetParameters() { return fWDcfg; };
  void SetParameters(Config_t par) { fWDcfg = par; };

  void OpenDigitizers();
  void CloseDigitizers();

  virtual void InitDigitizers();

  void Start();
  void Stop();

  void SendSWTrigger();

  void DumpRegisters();

  virtual void SetThreshold() = 0;

  // Memory
  virtual void AllocateMemory() = 0;
  virtual void FreeMemory() = 0;

  // Event readout
  std::shared_ptr<std::vector<std::shared_ptr<TreeData_t>>> GetData();
  virtual void ReadEvents() = 0;
  virtual void StartReadoutMT() = 0;
  virtual void StopReadoutMT() = 0;

  virtual void UseFineTS() = 0;
  virtual void UseHWFineTS() = 0;
  virtual void UseTrgCounter(const int mod, const int ch) = 0;

 protected:
  Config_t fWDcfg;     // acquisition parameters and user settings
  SysVars_t fSysVars;  // system variables
  int fHandler[MAX_NBRD];
  uint32_t fChMask[MAX_NBRD];

  void GetBoardInfo();
  int fTSample[MAX_NBRD];  // We need only this time sample information
  int fDigitizerModel;
  FirmWareCode fFirmware;
  uint32_t fNChs[MAX_NBRD];
  int fNBits;  // ADC, Waveform resolution

  // For Fine TS
  bool fFlagFineTS;
  bool fFlagHWFineTS;
  uint64_t fPreviousTime[MAX_NBRD][MAX_NCH];
  uint64_t fTimeOffset[MAX_NBRD][MAX_NCH];

  // For trg counter
  bool fFlagTrgCounter[MAX_NBRD][MAX_NCH];
  double fLostTrgCounter[MAX_NBRD][MAX_NCH];
  uint32_t fLostTrgCounterOffset[MAX_NBRD][MAX_NCH];

  std::shared_ptr<std::vector<std::shared_ptr<TreeData_t>>> fDataVec;

  typedef std::shared_ptr<std::vector<std::vector<char>>> RawData_t;
  std::deque<RawData_t> fRawDataQue;
  virtual void ReadRawData() = 0;
  virtual void ReadRawDataWrapper() = 0;
  std::thread fReadThread;
  virtual void DecodeRawData() = 0;
  virtual void DecodeRawDataWrapper() = 0;
  std::thread fDecodeThread;
  bool fReadoutFlag;
  std::mutex fMutex;
  const unsigned int fReadInterval = 100;
  const unsigned int fDecodeInterval = 10;

  void PrintError(const CAEN_DGTZ_ErrorCode &err,
                  const std::string &funcName = "") const;
};

#endif
