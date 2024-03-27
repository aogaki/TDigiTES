#include <CAENDigitizer.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <THttpServer.h>
#include <TSpline.h>
#include <TSystem.h>
#include <TTree.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "BoardUtils.h"
#include "Configure.h"
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TPHA.hpp"
#include "TPSD.hpp"
#include "TQDC.hpp"
#include "TWaveform.hpp"
#include "TreeData.h"
#include "digiTES.h"

enum class AppState { Quit, Reload, Continue };

AppState InputCHeck(void)
{
  struct termios oldt, newt;
  char ch;
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

  if (ch == 'q' || ch == 'Q') {
    return AppState::Quit;
  } else if (ch == 'r' || ch == 'R') {
    return AppState::Reload;
  }

  return AppState::Continue;
}

void InitDigitizer(TDigiTes *digitizer, const std::string &configFile)
{
  digitizer->LoadParameters(configFile);
  digitizer->OpenDigitizers();

  // Force mix mode (obtaining traces)
  auto par = digitizer->GetParameters();
  par.AcquisitionMode = ACQ_MODE_MIXED;

  digitizer->SetParameters(par);
  digitizer->InitDigitizers();
  digitizer->UseHWFineTS();
  digitizer->SetTraces();
  digitizer->AllocateMemory();
  digitizer->Start();
}

int main(int argc, char *argv[])
{
  // Check arguments
  std::string configFile;
  FirmWareCode fwType;
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " -f <configFile> -t <FW_Type>"
              << std::endl;
    return 1;
  } else {
    std::string fw;
    for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") {
        configFile = argv[i + 1];
      } else if (std::string(argv[i]) == "-t") {
        fw = argv[i + 1];
        if (fw == "DPP_PSD" || fw == "PSD") {
          fwType = FirmWareCode::DPP_PSD;
        } else if (fw == "DPP_PHA" || fw == "PHA") {
          fwType = FirmWareCode::DPP_PHA;
        } else if (fw == "DPP_QDC" || fw == "QDC") {
          fwType = FirmWareCode::DPP_QDC;
        } else {
          std::cerr << "Unknown firmware type: " << fw << std::endl;
          std::cerr
              << "Available types: DPP_PSD, PSD, DPP_PHA, PHA, DPP_QDC, QDC"
              << std::endl;
          return 1;
        }
      }
    }
    if (configFile.empty() || fw.empty()) {
      std::cerr << "Usage: " << argv[0] << " -f <config_file> -t <FW_Type>"
                << std::endl;
      return 1;
    }
  }

  auto server = new THttpServer("http:8080;noglobal;rw");

  TH1D *hist[16];
  TGraph *waveform[16];
  TGraph *CFDTrace[16];
  TGraph *longGate[16];
  TGraph *shortGate[16];
  TCanvas *canvas[16];
  for (int i = 0; i < 16; i++) {
    hist[i] = new TH1D(Form("hist_%d", i), Form("hist_%d", i), 35000, 0, 35000);
    server->Register("/", hist[i]);

    waveform[i] = new TGraph();
    waveform[i]->SetMarkerColor(kBlue);
    waveform[i]->SetLineColor(kBlue);
    waveform[i]->SetTitle(Form("Waveform_%d", i));
    waveform[i]->SetMinimum(0);
    waveform[i]->SetMaximum(18000);
    waveform[i]->SetPoint(0, 0, 0);

    CFDTrace[i] = new TGraph();
    CFDTrace[i]->SetMarkerColor(kRed);
    CFDTrace[i]->SetLineColor(kRed);
    CFDTrace[i]->SetTitle(Form("CFDTrace_%d", i));
    CFDTrace[i]->SetMinimum(0);
    CFDTrace[i]->SetMaximum(18000);
    CFDTrace[i]->SetPoint(0, 0, 0);

    longGate[i] = new TGraph();
    longGate[i]->SetMarkerColor(kGreen);
    longGate[i]->SetLineColor(kGreen);
    longGate[i]->SetTitle(Form("LongGate_%d", i));
    longGate[i]->SetMinimum(0);
    longGate[i]->SetMaximum(18000);
    longGate[i]->SetPoint(0, 0, 0);

    shortGate[i] = new TGraph();
    shortGate[i]->SetMarkerColor(kMagenta);
    shortGate[i]->SetLineColor(kMagenta);
    shortGate[i]->SetTitle(Form("ShortGate_%d", i));
    shortGate[i]->SetMinimum(0);
    shortGate[i]->SetMaximum(18000);
    shortGate[i]->SetPoint(0, 0, 0);

    canvas[i] =
        new TCanvas(Form("traces_%d", i), Form("traces_%d", i), 800, 600);
    server->Register("/", canvas[i]);
    canvas[i]->cd();
    waveform[i]->Draw("AL");
  }

  std::unique_ptr<TDigiTes> digitizer;
  if (fwType == FirmWareCode::DPP_PSD) {
    digitizer.reset(new TPSD);
  } else if (fwType == FirmWareCode::DPP_PHA) {
    digitizer.reset(new TPHA);
  } else if (fwType == FirmWareCode::DPP_QDC) {
    digitizer.reset(new TQDC);
  } else {
    std::cerr << "Unknown firmware type" << std::endl;
    return 1;
  }
  InitDigitizer(digitizer.get(), configFile);

  auto par = digitizer->GetParameters();
  auto dt = par.Tsampl;
  std::cout << "Time step: " << dt << " ns" << std::endl;

  while (true) {
    auto state = InputCHeck();
    if (state == AppState::Quit) {
      break;
    } else if (state == AppState::Reload) {
      InitDigitizer(digitizer.get(), configFile);
    }

    usleep(10);
    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    if (data->size() > 0) {
      for (auto &d : *data) {
        auto ch = d->Ch;
        if (ch < 0 || ch > 15) continue;
        hist[ch]->Fill(d->ChargeLong);

        for (auto iPoint = 0; iPoint < d->RecordLength; iPoint++) {
          if (fwType == FirmWareCode::DPP_PSD) {
            waveform[d->Ch]->SetPoint(iPoint, iPoint * dt, d->Trace1[iPoint]);
            CFDTrace[d->Ch]->SetPoint(iPoint, iPoint * dt, d->Trace2[iPoint]);
            longGate[d->Ch]->SetPoint(iPoint, iPoint * dt,
                                      d->DTrace1[iPoint] * 14000 + 3000);
            shortGate[d->Ch]->SetPoint(iPoint, iPoint * dt,
                                       d->DTrace2[iPoint] * 14000 + 2000);
          } else if (fwType == FirmWareCode::DPP_PHA) {
            ;
          } else if (fwType == FirmWareCode::DPP_QDC) {
            ;
          }
        }
      }

      for (auto iCh = 0; iCh < 16; iCh++) {
        canvas[iCh]->cd();

        if (fwType == FirmWareCode::DPP_PSD) {
          waveform[iCh]->Draw("AL");
          CFDTrace[iCh]->Draw("SAME");
          longGate[iCh]->Draw("SAME");
          shortGate[iCh]->Draw("SAME");
        } else if (fwType == FirmWareCode::DPP_PHA) {
          ;
        } else if (fwType == FirmWareCode::DPP_QDC) {
          ;
        }

        canvas[iCh]->Update();
      }
    }

    gSystem->ProcessEvents();
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  std::cout << "Finished" << std::endl;
  return 0;
}
