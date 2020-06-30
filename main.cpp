#include <CAENDigitizer.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <TSystem.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <iostream>
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
#include "TWaveForm.hpp"
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
  std::unique_ptr<TApplication> app(new TApplication("testApp", &argc, argv));

  // std::unique_ptr<TCanvas> canv(new TCanvas("canv", "test", 1600, 800));
  // canv->Divide(4, 2);
  //
  // constexpr int nGraphs = 8;
  // std::array<std::unique_ptr<TGraph>, nGraphs> grWave;
  // for (auto i = 0; i < nGraphs; i++) {
  //   grWave[i].reset(new TGraph);
  //   grWave[i]->SetTitle(Form("ch%02d", i));
  //   grWave[i]->SetMaximum(20000);
  //   grWave[i]->SetMinimum(0);
  // }

  // auto digitizer = new TDataTaking;
  // std::unique_ptr<TPSD> digitizer(new TPSD);
  std::unique_ptr<TWaveForm> digitizer(new TWaveForm);

  // digitizer->LoadParameters("PSD.conf");
  digitizer->LoadParameters();
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  digitizer->AllocateMemory();

  digitizer->Start();

  int counter = 0;
  while (true) {
    digitizer->SendSWTrigger();
    digitizer->ReadEvents();
    auto data = digitizer->GetData();

    auto nData = data->size();
    std::cout << nData << " hits" << std::endl;
    for (auto i = 0; i < nData; i++) {
      auto ch = data->at(i)->ChNumber;
      auto nPoints = data->at(i)->RecordLength;
      std::cout << nPoints << " points, " << nPoints * 2 / 1000. / 1000.
                << std::endl;
      // for (auto iPoint = 0; iPoint < nPoints; iPoint++)
      // grWave[ch]->SetPoint(iPoint, iPoint * 2, data->at(i)->Trace1[iPoint]);
    }
    /*
        for (auto iGr = 0; iGr < nGraphs; iGr++) {
          canv->cd(iGr + 1);
          if (grWave[iGr]->GetN() > 1)
            grWave[iGr]->Draw();
        }
        canv->Update();
    */
    if (InputCHeck()) {
      break;
    }
    // if (counter++ > 10)
    //   break;
    usleep(1000000);
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  std::cout << "Finished" << std::endl;
  return 0;
}

// /* Real data taking example */
// int main(int argc, char *argv[])
// {
//   std::unique_ptr<TApplication> app;
//
//   for (auto i = 1; i < argc; i++) {
//     if (std::string(argv[i]) == "-g")
//       app.reset(new TApplication("testApp", &argc, argv));
//   }
//
//   // auto digitizer = new TDataTaking;
//   std::unique_ptr<TDataTaking> digitizer(new TDataTaking);
//
//   digitizer->Start();
//
//   std::thread readDigitizer(&TDataTaking::ReadDigitizer, digitizer.get());
//   std::thread fillData(&TDataTaking::FillData, digitizer.get());
//
//   while (true) {
//     gSystem->ProcessEvents();  // This should be called at main thread
//
//     if (InputCHeck()) {
//       digitizer->Terminate();
//       readDigitizer.join();
//       fillData.join();
//       break;
//     }
//
//     usleep(1000);
//   }
//
//   digitizer->Stop();
//
//   // delete digitizer;
//
//   std::cout << "Finished" << std::endl;
//   return 0;
// }
