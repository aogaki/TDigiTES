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
#include "TDigiTes.hpp"
#include "TPHA.hpp"
#include "TPSD.hpp"
#include "TWaveform.hpp"
#include "TreeData.h"
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

int main(int argc, char *argv[])
{
  TApplication app("test", &argc, argv);
  auto hist = new TH1D("hist", "Charge long", 1024, 0.5, 1024.5);
  hist->SetXTitle("[ch]");
  auto waveform = new TGraph();
  waveform->SetMinimum(0);
  waveform->SetMaximum(18000);
  waveform->SetPoint(0, 0, 0);  // dummy
  auto canv = new TCanvas("canv", "test", 1200, 400);
  canv->Divide(2, 1);
  canv->cd(1);
  hist->Draw();
  canv->cd(2);
  waveform->Draw("AL");

  // std::unique_ptr<TWaveform> digitizer(new TWaveform);
  std::unique_ptr<TPSD> digitizer(new TPSD);
  // std::unique_ptr<TPHA> digitizer(new TPHA);

  // digitizer->LoadParameters();
  // digitizer->LoadParameters("./PHA.conf");
  digitizer->LoadParameters("./PSD.conf");
  // digitizer->LoadParameters("./Waveform.conf");
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  digitizer->UseFineTS();
  // digitizer->UseHWFineTS();
  digitizer->UseTrgCounter(0, 3);  // Change FineTS -> Lost trg counter
  digitizer->AllocateMemory();

  digitizer->Start();
  digitizer->StartReadoutMT();

  while (true) {
    // for (auto i = 0; i < 1000; i++) digitizer->SendSWTrigger();
    // usleep(1);

    // digitizer->ReadEvents();
    auto data = digitizer->GetData();
    auto nHits = data->size();

    constexpr auto updateTime = 1.e9;  // 1 sec in ns
    static double lastTS = 0.;
    static int counter = 0;
    static double lastLostCounter = 0.;
    for (auto iHit = 0; iHit < nHits; iHit++) {
      double ts = data->at(iHit)->TimeStamp;
      double lostCounter = data->at(iHit)->FineTS;
      auto adc = data->at(iHit)->ChargeLong;
      if (data->at(iHit)->Ch == 3) {
        hist->Fill(adc);
        for (auto iPoint = 0; iPoint < data->at(iHit)->RecordLength; iPoint++)
          waveform->SetPoint(iPoint, iPoint, data->at(iHit)->Trace1[iPoint]);
      }
      // auto lostTrg = uint16_t((data->at(iHit)->Extras >> 16) & 0xFFFF);
      // auto totalTrg = uint16_t(data->at(iHit)->Extras & 0xFFFF);
      //
      // std::cout << int(data->at(iHit)->Ch) << "\t" << lostTrg << "\t"
      //           << "\t" << fineTS << "\t" << totalTrg << "\t" << counter
      //           << std::endl;
      if (data->at(iHit)->Ch == 3) {
        counter++;
        if (ts - lastTS > updateTime) {
          auto eveRate = (counter / (ts - lastTS)) * 1.e9;
          auto lostRate =
              ((lostCounter - lastLostCounter) / (ts - lastTS)) * 1.e9;
          std::cout << eveRate << " Hz\t" << lostRate << " Hz\t"
                    << eveRate + lostRate << " Hz" << std::endl;
          lastTS = ts;
          lastLostCounter = lostCounter;
          counter = 0;
        }
      }
    }

    canv->Update();

    if (InputCHeck()) {
      break;
    }
  }

  digitizer->StopReadoutMT();
  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  std::cout << "Finished" << std::endl;
  app.Run();
  return 0;
}
