#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <TROOT.h>

#include <iostream>
#include <string>

#include "TDataTaking.hpp"

enum class AppState { Quit, Reload, Continue };

AppState InputCheck()
{
  struct termios oldt, newt;
  char ch = -1;
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

int main(int argc, char *argv[])
{
  ROOT::EnableImplicitMT();

  // Check arguments
  std::string configFile;
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <config file>" << std::endl;
    return 1;
  } else {
    configFile = argv[1];
  }

  auto dataTaking = std::make_unique<TDataTaking>(configFile);

  dataTaking->InitAndStart();

  while (true) {
    auto state = InputCheck();
    if (state == AppState::Quit) {
      break;
    } else if (state == AppState::Reload) {
      dataTaking->Stop();
      dataTaking->InitAndStart();
    }

    // dataTaking->DataProcess();
    gSystem->ProcessEvents();

    usleep(10);
  }

  dataTaking->Stop();

  return 0;
}
