#include <CAENDigitizer.h>
#include <TApplication.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TH1.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <iostream>
#include <memory>
#include <string>

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
  TApplication app("testApp", &argc, argv);

  std::unique_ptr<TDigiTes> digitizer(new TDigiTes());
  digitizer->LoadParameters();
  digitizer->InitDigitizers();

  digitizer->AllocateMemory();

  digitizer->Start();

  TH1D *hisCharge = new TH1D("hisCharge", "test", 2000, 0, 2000);
  TCanvas *canvas = new TCanvas();
  TGraph *grWave = new TGraph();
  grWave->SetMaximum(20000);
  grWave->SetMinimum(0);
  TCanvas *canvas2 = new TCanvas();
  canvas->cd();
  hisCharge->Draw();

  for (int i = 0; true; i++) {
    digitizer->ReadEvents();
    auto data = digitizer->GetData();
    const auto kHit = data->size();
    for (auto iHit = 0; iHit < kHit; iHit++) {
      if (data->at(iHit).ChNumber == 0) {
        hisCharge->Fill(data->at(iHit).ChargeShort);

        auto size = data->at(iHit).RecordLength;
        auto signal = data->at(iHit).WaveForm;

        for (auto i = 0; i < size; i++) {
          grWave->SetPoint(i, i * 2, signal[i]);
        }
      }
    }

    canvas2->cd();
    grWave->Draw("AC");
    canvas2->Update();

    canvas->cd();
    hisCharge->Draw();
    canvas->Update();

    if (testKbHit()) break;

    usleep(1000);
  }

  digitizer->Stop();

  digitizer->FreeMemory();
  return 0;
}
