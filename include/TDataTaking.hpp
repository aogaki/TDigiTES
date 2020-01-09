#ifndef TDataTaking_HPP
#define TDataTaking_HPP 1

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <THttpServer.h>
#include <TStyle.h>
#include <TTree.h>

#include "TDigiTes.hpp"
#include "TPSDData.hpp"

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
  TDigiTes *fDigitizer;

  std::deque<TPSDData_t> fQueue;
  TPSDData_t fData;
  std::mutex fMutex;

  bool fAcqFlag;

  // Something from ROOT
  void PlotAll();
  int fFillCounter;
  TH1D *fHisADC[8];
  TGraph *fGrWave;
  TCanvas *fCanvas;
  THttpServer *fServer;
  TTree *fTree;
};

#endif
