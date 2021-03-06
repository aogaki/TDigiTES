#include <CAENDigitizer.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <TSpline.h>
#include <TTree.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "BoardUtils.h"
#include "Configure.h"
#include "Console.h"
#include "ParamParser.h"
#include "TDataTaking.hpp"
#include "TDigiTes.hpp"
#include "TPSD.hpp"
#include "TPSDData.hpp"
#include "TWaveform.hpp"
#include "digiTES.h"

int InputCHeck(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

/* Only display the number of hit */
int main(int argc, char *argv[])
{
  std::unique_ptr<TApplication> app(new TApplication("app", &argc, argv));

  std::unique_ptr<TCanvas> canvGr(new TCanvas("canvGr", "Waveform"));
  std::unique_ptr<TGraph> gr1(new TGraph());
  gr1->SetLineColor(kBlue);
  gr1->SetMinimum(0);
  gr1->SetMaximum(18000);
  std::unique_ptr<TGraph> gr2(new TGraph());
  gr2->SetLineColor(kRed);
  std::unique_ptr<TGraph> gr3(new TGraph());
  gr3->SetLineColor(kGreen);
  std::unique_ptr<TGraph> gr4(new TGraph());
  gr4->SetLineColor(kBlack);

  std::unique_ptr<TCanvas> canvHist(
      new TCanvas("canvHist", "Fine TS", 1200, 500));
  canvHist->Divide(2, 1);

  // std::unique_ptr<TH2D> hist1(
  //     new TH2D("hist1", "Fine TS", 1024, -0.5, 1023.5, 1024, -0.5, 1023.5));
  // std::unique_ptr<TH2D> hist2(
  //     new TH2D("hist2", "Fine TS", 1024, -0.5, 1023.5, 1024, -0.5, 1023.5));
  // std::unique_ptr<TH1D> hist1(
  //     new TH1D("hist1", "Fine TS, ch2", 1024, -0.5, 1023.5));
  // std::unique_ptr<TH1D> hist2(
  //     new TH1D("hist2", "Fine TS, ch3", 1024, -0.5, 1023.5));
  std::unique_ptr<TH1D> hist1(
      new TH1D("hist1", "Fine TS, ch4", 2001, -0.5, 2000.5));
  std::unique_ptr<TH1D> hist2(
      new TH1D("hist2", "Fine TS, ch5", 2001, -0.5, 2000.5));
  canvHist->cd(1);
  hist1->Draw("COLZ");
  canvHist->cd(2);
  hist2->Draw("COLZ");

  // std::unique_ptr<TWaveform> digitizer(new TWaveform);
  // std::unique_ptr<TPSD> digitizer(new TPSD);
  std::unique_ptr<TPHA> digitizer(new TPHA);

  // digitizer->LoadParameters();
  // digitizer->LoadParameters("/DAQ/PSD.conf");
  digitizer->LoadParameters("./PHA.conf");
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  digitizer->UseFineTS();
  digitizer->AllocateMemory();

  digitizer->Start();

  auto cfg = digitizer->GetParameters();
  const auto timeStep = cfg.Tsampl;

  constexpr auto maxEve = 100000;
  auto eveCounter = 0;

  std::unique_ptr<TTree> tree(new TTree("FineTS", "fine timestamp test"));
  Int_t ch;
  tree->Branch("Ch", &ch, "Ch/I");
  Int_t brd;
  tree->Branch("Mod", &brd, "Mod/I");
  Int_t fineHW;
  tree->Branch("FineHW", &fineHW, "FineHW/I");
  Int_t fineTS;
  tree->Branch("FineTS", &fineTS, "FineTS/I");

  std::chrono::system_clock::time_point last, now;
  last = std::chrono::system_clock::now();

  while (true) {
    // for (auto i = 0; i < 10; i++) digitizer->SendSWTrigger();

    now = std::chrono::system_clock::now();
    auto time = now - last;
    last = now;
    auto nanosec =
        std::chrono::duration_cast<std::chrono::nanoseconds>(time).count();
    std::cout << nanosec << " ns" << std::endl;

    digitizer->ReadEvents();
    auto data = digitizer->GetData();

    std::vector<Double_t> xVec;
    std::vector<Double_t> yVec;

    auto nData = data->size();
    eveCounter += nData;
    std::cout << nData << " hits" << std::endl;
    for (auto i = 0; i < nData; i++) {
      // something
      brd = data->at(i)->ModNumber;
      ch = data->at(i)->ChNumber;
      auto nPoints = data->at(i)->RecordLength;
      auto charge = data->at(i)->Energy;

      // if (charge > 0 && charge < 30000 && (ch == 3 || ch == 2 || ch == 5)) {
      // if (charge > 0 && charge < 30000 && (ch == 4 || ch == 5)) {
      if (charge > 0 && (ch == 4 || ch == 5)) {
        auto triggerPos = 0;
        constexpr auto nSamples = 6;
        for (auto iData = 0; iData < nPoints; iData++) {
          gr1->SetPoint(iData, iData * timeStep, data->at(i)->Trace1[iData]);
          gr2->SetPoint(iData, iData * timeStep, data->at(i)->Trace2[iData]);
          gr3->SetPoint(iData, iData * timeStep,
                        data->at(i)->DTrace1[iData] * 15000);
          gr4->SetPoint(iData, iData * timeStep,
                        data->at(i)->DTrace2[iData] * 8192);
          if (data->at(i)->DTrace2[iData] != 0) {
            triggerPos = iData;
            xVec.clear();
            yVec.clear();
            for (auto iPoint = 0; iPoint < nSamples; iPoint++) {
              UInt_t index = iData + iPoint - nSamples / 2;
              xVec.push_back(index * 2);
              yVec.push_back(data->at(i)->Trace1[index]);
            }
          }
        }

        auto fineTS = data->at(i)->FineTS;
        // auto fineTS = 2000. * (data->at(i)->Extras & 0x3FF) / 1024.;
        if (fineTS != 0) {
          if (ch == 4) {
            hist1->Fill(fineTS);
          } else if (ch == 5) {
            hist2->Fill(fineTS);
          }
        }
        tree->Fill();
      }
    }
    canvHist->cd(1)->Modified();
    canvHist->cd(1)->Update();
    canvHist->cd(2)->Modified();
    canvHist->cd(2)->Update();

    if (gr1->GetN() > 0) {
      canvGr->cd();
      gr1->Draw("AL");
      gr2->Draw("SAME");
      gr3->Draw("SAME");
      gr4->Draw("SAME");
      canvGr->cd(1)->Modified();
      canvGr->cd(1)->Update();
    }

    // std::cout << "total " << eveCounter << " hits" << std::endl;
    // if (eveCounter > maxEve) break;

    if (InputCHeck()) {
      break;
    }

    usleep(10);
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  auto file = new TFile("result.root", "RECREATE");
  tree->Write();
  file->Close();
  delete file;

  app->Run();

  std::cout << "Finished" << std::endl;
  return 0;
}
