#include "TDataTaking.hpp"

#include <iostream>

TDataTaking::TDataTaking(const std::string &configFile)
    : fConfigFile(configFile)
{
  fDigitizer = std::make_unique<TWaveform>();
  fDigitizer->LoadParameters(fConfigFile);
  fDigitizer->OpenDigitizers();
  fDigitizer->GetBoardInfo();

  CheckFW();

  InitHists();
  InitSignals();
  InitCanvas();
  InitHttpServer();
}

TDataTaking::~TDataTaking()
{
  // Very bad way, but I have not yet found a good way to delete THttpServer
  //   delete fHttpServer;
}

// Public method
void TDataTaking::InitAndStart()
{
  ResetHists();
  ResetSignals();
  for (uint32_t iMod = 0; iMod < kgMod; iMod++) {
    for (uint32_t iCh = 0; iCh < kgCh; iCh++) {
      fCanvasArray[iMod][iCh]->cd();
      if (fInputArray[iMod][iCh]) fInputArray[iMod][iCh]->Draw("AL");
    }
  }

  fDigitizer->LoadParameters(fConfigFile);
  fDigitizer->OpenDigitizers();

  auto par = fDigitizer->GetParameters();
  // Force mix mode (obtaining traces)
  par.AcquisitionMode = ACQ_MODE_MIXED;
  // Force individual software trigger start
  par.StartMode = START_MODE_INDEP_SW;

  fTimeStep = par.Tsampl;

  fDigitizer->SetParameters(par);
  fDigitizer->InitDigitizers();
  fDigitizer->UseHWFineTS();
  fDigitizer->SetTraces();
  if (par.TrginMode == TRGIN_MODE_VETO) fDigitizer->SetTrginVETO();
  fDigitizer->AllocateMemory();
  fDigitizer->Start();
}

void TDataTaking::Stop()
{
  fDigitizer->Stop();
  fDigitizer->FreeMemory();
  fDigitizer->CloseDigitizers();
}

void TDataTaking::DataProcess()
{
  fDigitizer->ReadEvents();
  auto data = fDigitizer->GetData();
  if (data->size() > 0) {
    for (auto &d : *data) {
      auto ch = d->Ch;
      auto mod = d->Mod;
      if (ch < 0 || ch > kgCh || mod < 0 || mod > kgMod) continue;
      fHistArray[mod][ch]->Fill(d->ChargeLong);

      for (auto iPoint = 0; iPoint < d->RecordLength; iPoint++) {
        fInputArray[d->Mod][d->Ch]->SetPoint(iPoint, iPoint * fTimeStep,
                                             d->Trace1[iPoint]);
        fCFDArray[d->Mod][d->Ch]->SetPoint(iPoint, iPoint * fTimeStep,
                                           d->Trace2[iPoint]);
        fLongGateArray[d->Mod][d->Ch]->SetPoint(
            iPoint, iPoint * fTimeStep, d->DTrace1[iPoint] * 14000 + 3000);
        fShortGateArray[d->Mod][d->Ch]->SetPoint(
            iPoint, iPoint * fTimeStep, d->DTrace2[iPoint] * 14000 + 2000);
      }
    }

    for (auto iMod = 0; iMod < kgMod; iMod++) {
      for (auto iCh = 0; iCh < kgCh; iCh++) {
        fCanvasArray[iMod][iCh]->cd();
        fInputArray[iMod][iCh]->Draw("AL");
        fCFDArray[iMod][iCh]->Draw("SAME");
        fLongGateArray[iMod][iCh]->Draw("SAME");
        fShortGateArray[iMod][iCh]->Draw("SAME");
        fCanvasArray[iMod][iCh]->Update();
      }
    }
  }

  gSystem->ProcessEvents();
}

// Private method
void TDataTaking::CheckFW()
{
  auto fFWCode = fDigitizer->GetFirmware();
  if (fFWCode == FirmWareCode::DPP_PSD) {
    std::cout << "Firmware: DPP_PSD" << std::endl;
    fDigitizer.reset(new TPSD());
  } else if (fFWCode == FirmWareCode::DPP_PHA) {
    std::cout << "Firmware: DPP_PHA" << std::endl;
    fDigitizer.reset(new TPHA());
  } else if (fFWCode == FirmWareCode::DPP_QDC) {
    std::cout << "Firmware: DPP_QDC" << std::endl;
    fDigitizer.reset(new TQDC());
  } else {
    std::cerr << "Unknown firmwarem use waveform" << std::endl;
  }
}

void TDataTaking::InitHttpServer()
{
  fHttpServer = new THttpServer("http:8080;noglobal;rw");
  for (auto iBrd = 0; iBrd < kgMod; iBrd++) {
    TString regDirectory = Form("/Brd%02d", iBrd);
    for (auto iCh = 0; iCh < kgCh; iCh++) {
      if (fHistArray[iBrd][iCh])
        fHttpServer->Register(regDirectory, fHistArray[iBrd][iCh].get());
      if (fCanvasArray[iBrd][iCh])
        fHttpServer->Register(regDirectory, fCanvasArray[iBrd][iCh].get());
    }
  }
}

void TDataTaking::InitHists()
{
  constexpr double max = 32000;
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fHistArray[i][j] = std::make_unique<TH1D>(
          Form("hist_%02d_%02d", i, j), Form("Module %d Channel %d", i, j),
          max + 1, -0.5, max + 0.5);
    }
  }
}

void TDataTaking::ResetHists()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      if (fHistArray[i][j]) fHistArray[i][j]->Reset();
    }
  }
}

void TDataTaking::InitCanvas()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fCanvasArray[i][j] = std::make_unique<TCanvas>(
          Form("signal%02d_%02d", i, j), Form("Module %d Channel %d", i, j),
          800, 600);
    }
  }
}

void TDataTaking::InitSignals()
{
  InitInputArray();
  InitCFDArray();
  InitLongGateArray();
  InitShortGateArray();
}

void TDataTaking::InitInputArray()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fInputArray[i][j] = std::make_unique<TGraph>();
      fInputArray[i][j]->SetMarkerColor(kBlue);
      fInputArray[i][j]->SetLineColor(kBlue);
      fInputArray[i][j]->SetTitle(Form("Input_%02d_%02d", i, j));
      fInputArray[i][j]->SetMinimum(0);
      fInputArray[i][j]->SetMaximum(18000);
      fInputArray[i][j]->SetPoint(0, 0, 0);
    }
  }
}

void TDataTaking::InitCFDArray()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fCFDArray[i][j] = std::make_unique<TGraph>();
      fCFDArray[i][j]->SetMarkerColor(kRed);
      fCFDArray[i][j]->SetLineColor(kRed);
      fCFDArray[i][j]->SetTitle(Form("CFDTrace_%02d_%02d", i, j));
      fCFDArray[i][j]->SetMinimum(0);
      fCFDArray[i][j]->SetMaximum(18000);
      fCFDArray[i][j]->SetPoint(0, 0, 0);
    }
  }
}

void TDataTaking::InitLongGateArray()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fLongGateArray[i][j] = std::make_unique<TGraph>();
      fLongGateArray[i][j]->SetMarkerColor(kGreen);
      fLongGateArray[i][j]->SetLineColor(kGreen);
      fLongGateArray[i][j]->SetTitle(Form("LongGate_%02d_%02d", i, j));
      fLongGateArray[i][j]->SetMinimum(0);
      fLongGateArray[i][j]->SetMaximum(18000);
      fLongGateArray[i][j]->SetPoint(0, 0, 0);
    }
  }
}

void TDataTaking::InitShortGateArray()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      fShortGateArray[i][j] = std::make_unique<TGraph>();
      fShortGateArray[i][j]->SetMarkerColor(kMagenta);
      fShortGateArray[i][j]->SetLineColor(kMagenta);
      fShortGateArray[i][j]->SetTitle(Form("ShortGate_%02d_%02d", i, j));
      fShortGateArray[i][j]->SetMinimum(0);
      fShortGateArray[i][j]->SetMaximum(18000);
      fShortGateArray[i][j]->SetPoint(0, 0, 0);
    }
  }
}

void TDataTaking::ResetSignals()
{
  for (uint32_t i = 0; i < kgMod; i++) {
    for (uint32_t j = 0; j < kgCh; j++) {
      if (fInputArray[i][j]) {
        fInputArray[i][j]->Set(0);
        fInputArray[i][j]->SetPoint(0, 0, 0);
      }
      if (fCFDArray[i][j]) {
        fCFDArray[i][j]->Set(0);
        fCFDArray[i][j]->SetPoint(0, 0, 0);
      }
      if (fLongGateArray[i][j]) {
        fLongGateArray[i][j]->Set(0);
        fLongGateArray[i][j]->SetPoint(0, 0, 0);
      }
      if (fShortGateArray[i][j]) {
        fShortGateArray[i][j]->Set(0);
        fShortGateArray[i][j]->SetPoint(0, 0, 0);
      }
    }
  }
}
