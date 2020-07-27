#include <CAENDigitizer.h>
#include <TApplication.h>
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
#include "TWaveform.hpp"
#include "digiTES.h"

int InputCHeck(void) {
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

/* Real data taking example */
int main(int argc, char *argv[]) {
  std::unique_ptr<TApplication> app;

  std::unique_ptr<TDataTaking> digitizer(new TDataTaking);

  digitizer->Start();

  std::thread readDigitizer(&TDataTaking::ReadDigitizer, digitizer.get());
  std::thread fillData(&TDataTaking::FillData, digitizer.get());
  std::thread plotData(&TDataTaking::PlotData, digitizer.get());

  while (true) {
    // for (auto i = 0; i < 5; i++) {
    // digitizer->SendSWTrigger();
    //}

    gSystem->ProcessEvents(); // This should be called at main thread

    if (InputCHeck()) {
      // More option will be needed
      // Now only quit
      digitizer->Terminate();
      readDigitizer.join();
      fillData.join();
      plotData.join();
      break;
    }

    usleep(1000);
  }

  digitizer->Stop();

  std::cout << "Finished" << std::endl;
  return 0;
}
