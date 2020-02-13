#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>

#include "TDataTaking.hpp"

TDataTaking::TDataTaking() : fAcqFlag(true), fFillCounter(0)
{
  fDigitizer.reset(new TPSD());
  // fDigitizer.reset(new TPHA());
  fDigitizer->LoadParameters();
  fDigitizer->OpenDigitizers();
  fDigitizer->InitDigitizers();
  fDigitizer->AllocateMemory();
  auto par = fDigitizer->GetParameters();
  fTimeSample = par.Tsampl;

  // Something from ROOT
  gStyle->SetOptFit(1111);
  for (auto i = 0; i < 8; i++)
    fHisADC[i] = new TH1D(Form("hisADC%d", i), "Flux check", 33000, 0, 33000);

  fCanvas = new TCanvas("flux", "Flux check", 1600, 600);
  fCanvas->Divide(2, 1);
  fCanvas->cd(2)->SetGrid(kTRUE, kTRUE);
  fCanvas->cd();

  fGrWave = new TGraph();

  fGrDTrace1 = new TGraph();
  fGrDTrace1->SetLineColor(kRed);
  fGrDTrace1->SetFillColor(kRed);
  fGrDTrace1->SetFillStyle(3004);

  fGrDTrace2 = new TGraph();
  fGrDTrace2->SetLineColor(kBlue);
  fGrDTrace2->SetFillColor(kBlue);
  fGrDTrace2->SetFillStyle(3005);

  for (uint iSample = 0; iSample < par.RecordLength; iSample++) {
    fGrWave->SetPoint(iSample, iSample * fTimeSample, 8000);
    fGrDTrace1->SetPoint(iSample, iSample * fTimeSample, 8000);
    fGrDTrace2->SetPoint(iSample, iSample * fTimeSample, 8000);
  }
  fGrWave->SetMaximum(20000);
  fGrWave->SetMinimum(0);

  fServer.reset(new THttpServer("http:8888"));

  fData.reset(new TPSDData(8192));  // 8192 has no reason.  Just big buffer
  fTree = new TTree("data", "Neutron detector test");
  fTree->Branch("Ch", &(fData->ChNumber), "ChNumber/b");
  fTree->Branch("ChargeShort", &(fData->ChargeShort), "ChargeShort/S");
  fTree->Branch("ChargeLong", &(fData->ChargeLong), "ChargeLong/S");
  fTree->Branch("TimeStamp", &(fData->TimeStamp), "TimeStamp/l");
  fTree->Branch("RecordLength", &(fData->RecordLength), "RecordLength/i");
  fTree->Branch("WaveForm", fData->Trace1, "WaveForm[RecordLength]/s");
  fTree->Branch("Trace2", fData->Trace2, "Trace2[RecordLength]/s");
  fTree->Branch("DTrace1", fData->DTrace1, "DTrace1[RecordLength]/b");
  fTree->Branch("DTrace2", fData->DTrace2, "DTrace2[RecordLength]/b");
  fTree->Branch("DTrace3", fData->DTrace3, "DTrace3[RecordLength]/b");
  fTree->Branch("DTrace4", fData->DTrace4, "DTrace4[RecordLength]/b");

  // fData.reset(new TPHAData(8192));  // 8192 has no reason.  Just big buffer
  // fTree = new TTree("data", "Neutron detector test");
  // fTree->Branch("Ch", &(fData->ChNumber), "ChNumber/b");
  // fTree->Branch("Energy", &(fData->Energy), "Energy/S");
  // fTree->Branch("TimeStamp", &(fData->TimeStamp), "TimeStamp/l");
  // fTree->Branch("RecordLength", &(fData->RecordLength), "RecordLength/i");
  // fTree->Branch("WaveForm", fData->Trace1, "WaveForm[RecordLength]/s");
  // fTree->Branch("Trace2", fData->Trace2, "Trace2[RecordLength]/s");
  // fTree->Branch("DTrace1", fData->DTrace1, "DTrace1[RecordLength]/b");
  // fTree->Branch("DTrace2", fData->DTrace2, "DTrace2[RecordLength]/b");

  PlotAll();
}

TDataTaking::~TDataTaking()
{
  std::cout << "Start to finish the run." << std::endl;
  fDigitizer->FreeMemory();
  fDigitizer->CloseDigitizers();

  delete fCanvas;
  // delete fHisADC;
  for (auto &&his : fHisADC) delete his;
  delete fGrWave;
  delete fGrDTrace1;
  delete fGrDTrace2;

  // std::cout << "delete server" << std::endl;
  // delete fServer;

  std::cout << "Start to write the output file" << std::endl;
  TFile *file = new TFile(Form("%ld.root", time(nullptr)), "RECREATE");
  fTree->Write();
  file->Close();
  delete file;
  delete fTree;
  std::cout << "Finished to write the output file" << std::endl;
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

  std::cout << "Data read out finished" << std::endl;
}

void TDataTaking::FillData()
{
  while (fAcqFlag) {
    while (!fQueue.empty()) {
      fData->ModNumber = fQueue.front()->ModNumber;
      fData->ChNumber = fQueue.front()->ChNumber;
      fData->TimeStamp = fQueue.front()->TimeStamp;
      fData->ChargeLong = fQueue.front()->ChargeLong;
      fData->ChargeShort = fQueue.front()->ChargeShort;
      // fData->Energy = fQueue.front()->Energy;
      fData->RecordLength = fQueue.front()->RecordLength;
      constexpr auto eleSize = sizeof(*TPSDData_t::Trace1);
      memcpy(fData->Trace1, fQueue.front()->Trace1,
             fData->RecordLength * eleSize);

      constexpr auto eleSizeShort = sizeof(*fData->Trace1);
      memcpy(fData->Trace1, fQueue.front()->Trace1,
             fData->RecordLength * eleSizeShort);
      memcpy(fData->Trace2, fQueue.front()->Trace2,
             fData->RecordLength * eleSizeShort);

      constexpr auto eleSizeChar = sizeof(*fData->DTrace1);
      memcpy(fData->DTrace1, fQueue.front()->DTrace1,
             fData->RecordLength * eleSizeChar);
      memcpy(fData->DTrace2, fQueue.front()->DTrace2,
             fData->RecordLength * eleSizeChar);

      fTree->Fill();
      // fHisADC[fData->ChNumber]->Fill(fData->Energy);
      fHisADC[fData->ChNumber]->Fill(fData->ChargeLong);

      for (uint iSample = 0; iSample < fData->RecordLength; iSample++) {
        fGrWave->SetPoint(iSample, iSample * fTimeSample,
                          fData->Trace1[iSample]);
        fGrDTrace1->SetPoint(iSample, iSample * fTimeSample,
                             fData->DTrace1[iSample] * 20000);
        fGrDTrace2->SetPoint(iSample, iSample * fTimeSample,
                             fData->DTrace2[iSample] * 20000);
      }

      fMutex.lock();
      delete fQueue.front();
      fQueue.pop_front();
      fMutex.unlock();
    }

    if ((++fFillCounter % 1000) == 0) {
      PlotAll();
    }

    usleep(1000);
  }

  std::cout << "Data filling finished" << std::endl;
}

void TDataTaking::PlotAll()
{
  fCanvas->cd(1);
  fHisADC[1]->Draw();
  fCanvas->cd(2);
  fGrWave->Draw("AL");
  fGrDTrace1->Draw("SAME F");
  fGrDTrace2->Draw("SAME F");
  fCanvas->cd();
  fCanvas->Update();
}
