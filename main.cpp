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

int main(int argc, char *argv[])
{
  TApplication app("test", &argc, argv);
  // const auto binW = 2000. / 1024.;
  const auto binW = 2000. / 1000.;
  const auto nBins = int(2000 / binW) + 1;
  const auto start = binW / 2.;
  const auto end = start + (nBins * binW);
  auto hist = new TH1D("hist", "Fine TS", nBins, start, end);
  hist->SetXTitle("[ps]");
  auto canv = new TCanvas("canv", "test");
  hist->Draw();

  // std::unique_ptr<TWaveform> digitizer(new TWaveform);
  std::unique_ptr<TPSD> digitizer(new TPSD);
  // std::unique_ptr<TPHA> digitizer(new TPHA);

  // digitizer->LoadParameters();
  // digitizer->LoadParameters("/DAQ/PSD.conf");
  //digitizer->LoadParameters("./PHA.conf");
  digitizer->LoadParameters("./PSD.conf");
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  digitizer->UseFineTS();
  // digitizer->UseHWFineTS();
  digitizer->UseTrgCounter(0, 3);
  digitizer->AllocateMemory();

  digitizer->Start();

  while (true) {
    // for (auto i = 0; i < 1000; i++) digitizer->SendSWTrigger();
    usleep(10);

    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    auto nHits = data->size();

    constexpr auto updateTime = 1.e9;  // 1 sec in ns
    static double lastTime = 0.;
    static int counter = 0;
    static double lastLostCounter = 0.;
    for (auto iHit = 0; iHit < nHits; iHit++) {
      double ts = data->at(iHit)->TimeStamp;
      double fineTS = data->at(iHit)->FineTS;
      if (data->at(iHit)->ChNumber == 5) hist->Fill(fineTS);

      // auto lostTrg = uint16_t((data->at(iHit)->Extras >> 16) & 0xFFFF);
      // auto totalTrg = uint16_t(data->at(iHit)->Extras & 0xFFFF);
      //
      // std::cout << int(data->at(iHit)->ChNumber) << "\t" << lostTrg << "\t"
      //           << "\t" << fineTS << "\t" << totalTrg << "\t" << counter
      //           << std::endl;
      if (data->at(iHit)->ChNumber == 3) {
        counter++;
        if (ts - lastTime > updateTime) {
          auto eveRate = (counter / (ts - lastTime)) * 1.e9;
          auto lostRate = ((fineTS - lastLostCounter) / (ts - lastTime)) * 1.e9;
          // std::cout << counter << "\t" << ts << "\t" << lastTime << std::endl;
          std::cout << eveRate << " Hz\t" << lostRate << " Hz\t"
                    << eveRate + lostRate << " Hz" << std::endl;
          lastTime = ts;
          lastLostCounter = fineTS;
          counter = 0;
        }
      }
    }

    canv->Update();

    if (InputCHeck()) {
      break;
    }
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  app.Run();

  std::cout << "Finished" << std::endl;
  return 0;
}
