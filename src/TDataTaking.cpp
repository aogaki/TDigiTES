#include <TSystem.h>
#include <string.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#include "TDataTaking.hpp"
// #include "TPHA.h"

TDataTaking::TDataTaking() : fAcqFlag(true), fDataTakingFlag(true) {
  fDigitizer.reset(new TWaveform());
  fDigitizer->LoadParameters();
  fDigitizer->OpenDigitizers();
  fDigitizer->InitDigitizers();
  fDigitizer->AllocateMemory();
  auto par = fDigitizer->GetParameters();
  fTimeSample = par.Tsampl;

  // Something from ROOT
  gStyle->SetOptFit(1111);

  // fServer.reset(new THttpServer("http:8888"));
  fServer = new THttpServer("http:8888;noglobal");
  fServer->SetItemField("/", "_monitoring", "1000");

  for (auto iMod = 0; iMod < nMod; iMod++) {
    for (auto iCh = 0; iCh < nCh; iCh++) {
      fGraph[iMod][iCh] = new TGraph();
      fGraph[iMod][iCh]->SetMaximum(20000);
      fGraph[iMod][iCh]->SetMinimum(0);
      fGraph[iMod][iCh]->SetTitle(
          Form("WaveformMod%02dCh%02d;[ms]", iMod, iCh));

      fCanvas[iMod][iCh] =
          new TCanvas(Form("CanvasMod%02dCh%02d", iMod, iCh), "plot");

      fServer->Register(Form("Mod%d", iMod), fCanvas[iMod][iCh]);
    }
  }

  fTree.reset(new TTree("data", "detector waveform"));
  fTree->Branch("Mod", &(fModNumber), "ModNumber/b");
  fTree->Branch("Ch", &(fChNumber), "ChNumber/b");
  fTree->Branch("TimeStamp", &(fTimeStamp), "TimeStamp/l");
  fTree->Branch("RecordLength", &(fRecordLength), "RecordLength/i");
  fRecordLength = par.RecordLength;
  fTrace1.resize(fRecordLength);
  fTree->Branch("Waveform", &fTrace1[0], "Waveform[RecordLength]/s");

  for (auto iMod = 0; iMod < nMod; iMod++) {
    for (auto iCh = 0; iCh < nCh; iCh++) {
      for (uint iSample = 0; iSample < fRecordLength; iSample++) {
        fGraph[iMod][iCh]->SetPoint(iSample, iSample * fTimeSample, 0);
      }
      fCanvas[iMod][iCh]->cd();
      fGraph[iMod][iCh]->Draw("AL");
    }
  }
  PlotAll();
}

TDataTaking::~TDataTaking() {
  std::cout << "Start to finish the run." << std::endl;
  fDigitizer->FreeMemory();
  fDigitizer->CloseDigitizers();

  for (auto iMod = 0; iMod < nMod; iMod++) {
    for (auto iCh = 0; iCh < nCh; iCh++) {
      fServer->Unregister(fCanvas[iMod][iCh]);

      delete fGraph[iMod][iCh];
      delete fCanvas[iMod][iCh];
    }
  }

  std::cout << "Start to write the output file" << std::endl;
  TFile *file = new TFile(Form("%ld.root", time(nullptr)), "RECREATE");
  fTree->Write();
  file->Close();
  delete file;
  std::cout << "Finished to write the output file" << std::endl;

  // Reset does not work well.  Probably THttpServer has a problem.
  // Also sometimes release() also does not work....
  // fServer.reset(nullptr);
  // auto pointer = fServer.release();
  // delete pointer;

  // Force to exit
  gSystem->Exit(0);

  delete fServer;
  std::cout << "Monitor server killed" << std::endl;
}

void TDataTaking::ReadDigitizer() {
  while (fAcqFlag) {
    fDigitizer->ReadEvents();

    auto dataArray = fDigitizer->GetData();
    const int nHit = dataArray->size();
    for (int i = 0; i < nHit; i++) {
      // WaveformData_t data(dataArray->at(i)->RecordLength);
      SampleData_t data;
      data.Trace1.resize(dataArray->at(i)->RecordLength);
      data.ModNumber = dataArray->at(i)->ModNumber;
      data.ChNumber = dataArray->at(i)->ChNumber;
      data.TimeStamp = dataArray->at(i)->TimeStamp;
      data.RecordLength = dataArray->at(i)->RecordLength;
      std::memcpy(&data.Trace1[0], dataArray->at(i)->Trace1,
                  dataArray->at(i)->RecordLength * sizeof(data.Trace1[0]));

      fMutex.lock();
      fFillQueue.push_back(data);
      fPlotQueue.push_back(data);
      fMutex.unlock();
    }
    usleep(1);
  }

  fMutex.lock();
  fDataTakingFlag = false;
  std::cout << "Data read out finished" << std::endl;
  fMutex.unlock();
}

void TDataTaking::FillData() {
  while (fAcqFlag || fDataTakingFlag) { // Think carefully.  Not good enough
    while (!fFillQueue.empty()) {
      fModNumber = fFillQueue.front().ModNumber;
      fChNumber = fFillQueue.front().ChNumber;
      fTimeStamp = fFillQueue.front().TimeStamp;
      fRecordLength = fFillQueue.front().RecordLength;
      memcpy(&fTrace1[0], &(fFillQueue.front().Trace1[0]),
             fRecordLength * sizeof(fTrace1[0]));

      fTree->Fill();

      fMutex.lock();
      fFillQueue.pop_front();
      fMutex.unlock();
    }

    usleep(1);
  }

  fMutex.lock();
  std::cout << "Data filling finished" << std::endl;
  fMutex.unlock();
}

void TDataTaking::PlotData() {
  while (fAcqFlag) {
    while (!fPlotQueue.empty()) {
      // if (fAcqFlag == false) break;

      const auto mod = fPlotQueue.front().ModNumber;
      const auto ch = fPlotQueue.front().ChNumber;
      const auto nPoints = fPlotQueue.front().RecordLength;

      fGraph[mod][ch]->Set(0);
      constexpr uint step = 500;
      for (uint iSample = 0, counter = 0; iSample < nPoints; iSample += step) {
        fGraph[mod][ch]->SetPoint(counter++, iSample * fTimeSample,
                                  fPlotQueue.front().Trace1[iSample]);
      }
      fCanvas[mod][ch]->cd();
      fCanvas[mod][ch]->Modified();
      fCanvas[mod][ch]->Update();
      // fGraph[mod][ch]->Draw("AL");

      fMutex.lock();
      fPlotQueue.pop_front();
      fMutex.unlock();
    }

    usleep(1);
  }

  fMutex.lock();
  std::cout << "Data plotting finished" << std::endl;
  fMutex.unlock();
}

void TDataTaking::PlotAll() {
  for (auto iMod = 0; iMod < nMod; iMod++) {
    for (auto iCh = 0; iCh < nCh; iCh++) {
      fCanvas[iMod][iCh]->cd();
      fGraph[iMod][iCh]->Draw("AL");
    }
  }
}
