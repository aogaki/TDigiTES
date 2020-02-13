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

#include "TPHA.hpp"
#include "TPSD.hpp"

class TDataTaking
{
 public:
  TDataTaking();
  ~TDataTaking();

  void Start() { fDigitizer->Start(); };
  void Stop() { fDigitizer->Stop(); };

  void ReadDigitizer();

  void FillData();

  void Terminate() { fAcqFlag = false; };

 private:
  std::unique_ptr<TPSD> fDigitizer;
  // std::unique_ptr<TPHA> fDigitizer;
  int fTimeSample;

  std::deque<TPSDData_t *> fQueue;
  std::unique_ptr<TPSDData_t> fData;
  // std::deque<TPHAData_t *> fQueue;
  // std::unique_ptr<TPHAData_t> fData;
  std::mutex fMutex;

  bool fAcqFlag;

  // Something from ROOT
  void PlotAll();
  int fFillCounter;
  TH1D *fHisADC[8];
  TCanvas *fCanvas;
  TTree *fTree;

  // THttpServer *fServer;
  std::unique_ptr<THttpServer> fServer;

  TGraph *fGrWave;
  TGraph *fGrDTrace1;
  TGraph *fGrDTrace2;
};

#endif
