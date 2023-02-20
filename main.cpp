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
#include "ParamParser.h"
#include "TDigiTes.hpp"
#include "TPHA.hpp"
#include "TPSD.hpp"
#include "TQDC.hpp"
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
  std::unique_ptr<TQDC> digitizer(new TQDC);
  digitizer->LoadParameters("./QDC.conf");
  digitizer->OpenDigitizers();
  digitizer->InitDigitizers();
  // digitizer->Test();
  digitizer->AllocateMemory();
  digitizer->Start();

  // for (auto i = 0; i < 100; i++) {
  //   digitizer->SendSWTrigger();
  //   sleep(4);
  //   digitizer->ReadEvents();
  //   auto data = digitizer->GetData();
  //   std::cout << "Size" << data->size() << std::endl;
  // }

  while (true) {
    sleep(1);
    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    if (data->size() > 0) std::cout << "Size" << data->size() << std::endl;
    if (InputCHeck()) break;
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  digitizer->CloseDigitizers();

  std::cout << "Finished" << std::endl;
  return 0;
}
