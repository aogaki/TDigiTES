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
  digitizer->AllocateMemory();

  digitizer->Start();

  while (true) {
    // for (auto i = 0; i < 1000; i++) digitizer->SendSWTrigger();
    usleep(1000);

    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    // std::cout << data->size() <<" hits"<< std::endl;
    if (data->size() > 0) {
      std::cout << data->at(0)->ChargeLong << std::endl;
    }

    if (InputCHeck()) {
      break;
    }
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  std::cout << "Finished" << std::endl;
  return 0;
}
