#include <fstream>
#include <iostream>

#include "TDataTaking.hpp"

TDataTaking::TDataTaking() : fAcqFlag(true), fFillCounter(0)
{
  fDigitizer = new TDigiTes();
  fDigitizer->LoadParameters();
  fDigitizer->InitDigitizers();
  fDigitizer->AllocateMemory();

  // Something from ROOT
  gStyle->SetOptFit(1111);
  for (auto i = 0; i < 8; i++)
    fHisADC[i] = new TH1D(Form("hisADC%d", i), "Flux check", 33000, 0, 33000);

  fCanvas = new TCanvas("flux", "Flux check", 1600, 600);
  fCanvas->Divide(2, 1);
  fCanvas->cd(2)->SetGrid(kTRUE, kTRUE);
  fCanvas->cd();

  fGrWave = new TGraph();
  for (uint iSample = 0; iSample < 100; iSample++)
    fGrWave->SetPoint(iSample, iSample * 2, 8000);  // one sample 2 ns
  fGrWave->SetMaximum(20000);
  fGrWave->SetMinimum(0);

  // auto fServer = new THttpServer("http:8888?monitoring=5000;rw;noglobal");
  fServer = new THttpServer("http:8888");
  // fServer->Register("/", fHisADC);

  fTree = new TTree("data", "Neutron detector test");
  fTree->Branch("Ch", &fData.ChNumber, "fData.ChNumber/b");
  fTree->Branch("ChargeShort", &fData.ChargeShort, "fData.ChargeShort/S");
  fTree->Branch("ChargeLong", &fData.ChargeLong, "fData.ChargeLong/S");
  fTree->Branch("TimeStamp", &fData.TimeStamp, "fData.TimeStamp/l");
  fTree->Branch("RecordLength", &fData.RecordLength, "fData.RecordLength/i");
  fTree->Branch("WaveForm", fData.WaveForm,
                "fData.WaveForm[fData.RecordLength]/s");

  PlotAll();
}

TDataTaking::~TDataTaking()
{
  fDigitizer->FreeMemory();
  delete fDigitizer;

  // fServer->Unregister(fHisADC);
  // fServer->SetTerminate();
  delete fServer;
  delete fCanvas;
  // delete fHisADC;
  for (auto &&his : fHisADC) delete his;
  delete fGrWave;

  TFile *file = new TFile(Form("%ld.root", time(nullptr)), "RECREATE");
  fTree->Write();
  file->Close();
  delete file;
  delete fTree;
}

void TDataTaking::ReadDigitizer()
{
  while (fAcqFlag) {
    fDigitizer->ReadEvents();

    auto dataArray = fDigitizer->GetData();
    const int nHit = dataArray->size();
    // std::cout << nHit << std::endl;
    for (int i = 0; i < nHit; i++) {
      fMutex.lock();
      fQueue.push_back(dataArray->at(i));
      fMutex.unlock();
    }
    usleep(1000);
  }
}

void TDataTaking::FillData()
{
  while (fAcqFlag) {
    while (!fQueue.empty()) {
      fData = fQueue.front();
      fTree->Fill();
      fHisADC[fData.ChNumber]->Fill(fData.ChargeLong);

      for (uint iSample = 0; iSample < fData.RecordLength; iSample++) {
        fGrWave->SetPoint(iSample, iSample * 2, fData.WaveForm[iSample]);
      }

      fMutex.lock();
      fQueue.pop_front();
      fMutex.unlock();
    }

    if ((++fFillCounter % 100) == 0) {
      PlotAll();
    }

    usleep(1000);
  }
}

void TDataTaking::PlotAll()
{
  // Drawing makes crash.
  // Sometimes mutex lock solve this problem.
  fMutex.lock();
  fCanvas->cd(1);
  fHisADC[0]->Draw();
  fCanvas->cd(2);
  fGrWave->Draw("AL");
  fCanvas->cd();
  fCanvas->Update();
  fMutex.unlock();
}
