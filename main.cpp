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
#include "TPHA.hpp"
#include "TPHAData.hpp"
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
  std::unique_ptr<TPSD> digitizer(new TPSD);
  digitizer->LoadParameters("./PSD.conf");
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  // digitizer->UseFineTS();
  digitizer->AllocateMemory();

  digitizer->Start();

  auto cfg = digitizer->GetParameters();
  const auto timeStep = cfg.Tsampl;

  constexpr auto maxEve = 100000;
  auto eveCounter = 0;

  while (true) {
    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    auto nData = data->size();
    // std::cout << nData << " hits" << std::endl;

    for (auto i = 0; i < nData; i++) {
    }

    if (InputCHeck()) {
      break;
    }

    usleep(10);
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();
  return 0;
}
