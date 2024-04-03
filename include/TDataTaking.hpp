#ifndef TDataTaking_hpp
#define TDataTaking_hpp 1

// Not good example.  But you can understand what we need to do.

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>
#include <TSystem.h>

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "TDigiTes.hpp"
#include "TPHA.hpp"
#include "TPSD.hpp"
#include "TQDC.hpp"
#include "TWaveform.hpp"

constexpr uint32_t kgMod = 8;
constexpr uint32_t kgCh = 16;

class TDataTaking
{
 public:
  TDataTaking(const std::string &configFile);
  ~TDataTaking();

  void InitAndStart();
  void Stop();

  void DataProcess();

 private:
  std::unique_ptr<TDigiTes> fDigitizer;
  std::string fConfigFile;
  FirmWareCode fFWCode;
  int32_t fTimeStep;

  THttpServer *fHttpServer;
  void InitHttpServer();

  std::array<std::array<std::unique_ptr<TH1D>, kgCh>, kgMod> fHistArray;
  void InitHists();
  void ResetHists();

  std::array<std::array<std::unique_ptr<TCanvas>, kgCh>, kgMod> fCanvasArray;
  void InitCanvas();

  // Those names are representd for PSD.  PHA uses CFD as Input, Input as Trapezoid
  void InitSignals();
  void ResetSignals();
  std::array<std::array<std::unique_ptr<TGraph>, kgCh>, kgMod> fInputArray;
  void InitInputArray();
  std::array<std::array<std::unique_ptr<TGraph>, kgCh>, kgMod> fCFDArray;
  void InitCFDArray();
  std::array<std::array<std::unique_ptr<TGraph>, kgCh>, kgMod> fLongGateArray;
  void InitLongGateArray();
  std::array<std::array<std::unique_ptr<TGraph>, kgCh>, kgMod> fShortGateArray;
  void InitShortGateArray();

  // For MT
  std::vector<std::unique_ptr<std::vector<std::unique_ptr<TreeData_t>>>>
      fDataVec;
  std::mutex fDataMutex;
  void StartThreads();
  void StopThreads();

  std::thread fDataProcessThread;
  bool fDataProcessThreadFlag;
  void DataProcessThread();

  std::thread fDataReadThread;
  bool fDataReadThreadFlag;
  void DataReadThread();

  void CheckFW();
};

#endif
