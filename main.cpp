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
#include <memory>
#include <string>
#include <thread>

#include "BoardUtils.h"
#include "Configure.h"
#include "Console.h"
// #include "DataFiles.h"
// #include "Histograms.h"
#include "ParamParser.h"
// #include "Plots.h"
// #include "PreProcess.h"
// #include "Queues.h"
// #include "Readout.h"
// #include "Statistics.h"
// #include "ZCcal.h"
#include "digiTES.h"
// #include "fft.h"

#include "TDataTaking.hpp"
#include "TDigiTes.hpp"
#include "TPSDData.hpp"

int testKbHit(void)
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
  // TApplication app("testApp", &argc, argv);

  auto digitizer = new TDataTaking;

  digitizer->Start();

  std::thread readDigitizer(&TDataTaking::ReadDigitizer, digitizer);
  std::thread fillData(&TDataTaking::FillData, digitizer);

  while (true) {
    gSystem->ProcessEvents();  // This should be called at main thread

    if (testKbHit()) {
      digitizer->Terminate();
      readDigitizer.join();
      fillData.join();
      break;
    }

    usleep(1000);
  }

  digitizer->Stop();

  delete digitizer;
  return 0;
}
