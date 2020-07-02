#ifndef TDataTaking_HPP
#define TDataTaking_HPP 1

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>
#include <TStyle.h>
#include <TTree.h>

#include <memory>
#include <vector>

#include "TPHA.hpp"
#include "TPSD.hpp"
#include "TWaveform.hpp"
#include "TWaveformData.hpp"

struct SampleData_t {
  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint32_t RecordLength;
  std::vector<uint16_t> Trace1;
};

class TDataTaking
{
 public:
  TDataTaking();
  ~TDataTaking();

  void Start() { fDigitizer->Start(); };
  void Stop() { fDigitizer->Stop(); };

  void ReadDigitizer();
  void FillData();
  void PlotData();

  void Terminate() { fAcqFlag = false; };

  void SendSWTrigger() { fDigitizer->SendSWTrigger(); };

 private:
  // Where is the polymorphism?  Shame on me!!!
  std::unique_ptr<TWaveform> fDigitizer;
  // std::unique_ptr<TPSD> fDigitizer;
  // std::unique_ptr<TPHA> fDigitizer;
  int fTimeSample;

  std::deque<SampleData_t> fFillQueue;
  std::deque<SampleData_t> fPlotQueue;
  std::mutex fMutex;
  unsigned char fModNumber;
  unsigned char fChNumber;
  uint64_t fTimeStamp;
  uint32_t fRecordLength;
  std::vector<uint16_t> fTrace1;

  bool fAcqFlag;
  bool fDataTakingFlag;

  // Something from ROOT
  void PlotAll();
  std::unique_ptr<TTree> fTree;

  // std::unique_ptr<THttpServer> fServer;
  THttpServer *fServer;

  static constexpr Int_t nMod = 3;
  static constexpr Int_t nCh = 16;
  TCanvas *fCanvas[nMod][nCh];
  TGraph *fGraph[nMod][nCh];
};

#endif
