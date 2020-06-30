#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "ErrorCodeMap.hpp"
#include "TDigiTes.hpp"

TDigiTes::TDigiTes(){};

TDigiTes::~TDigiTes(){};

void TDigiTes::LoadParameters(std::string fileName)
{
  LoadSysVars(fSysVars);

  // auto configFileName = "digiTES_Config.txt";
  std::cout << "Open the input file " << fileName << std::endl;
  FILE *f_ini = fopen(fileName.c_str(), "r");

  if (f_ini == NULL) {
    std::cerr << "ERROR: Can't open configuration file " << fileName
              << std::endl;
    exit(1);
  }
  ParseConfigFile(f_ini, &fWDcfg, fSysVars);

  fclose(f_ini);

  // for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) fHandler[iBrd] = handle[iBrd];
}

void TDigiTes::OpenDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = CAEN_DGTZ_OpenDigitizer((CAEN_DGTZ_ConnectionType)fWDcfg.LinkType[b],
                                  fWDcfg.LinkNum[b], fWDcfg.ConetNode[b],
                                  fWDcfg.BaseAddress[b], &fHandler[b]);
    std::cout << fHandler[b] << "\t" << handle[b] << std::endl;
    handle[b] = fHandler[b];
    char cstr[500];
    if (ReadBoardInfo(b, cstr, fWDcfg) < 0) {
      std::cout << "ReadBoardInfo failed" << std::endl;
    }
    for (auto i = fWDcfg.NumAcqCh; i < MAX_NCH; i++)
      fWDcfg.EnableInput[b][i] = 0;

    SetTraceNames(fWDcfg);
  }
}

void TDigiTes::CloseDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = CAEN_DGTZ_CloseDigitizer(fHandler[b]);
  }
}

void TDigiTes::InitDigitizers()
{
  CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer(b, false, fWDcfg, fSysVars);
    if (ret != CAEN_DGTZ_Success) {
      std::cout << "ERROR: Failed to program the digitizer" << std::endl;
      exit(1);
    }
    SetVirtualProbes(b, fWDcfg);
  }

  GetBoardInfo();
}

void TDigiTes::GetBoardInfo()
{
  CAEN_DGTZ_BoardInfo_t info;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    // Check ch mask
    auto err = CAEN_DGTZ_GetChannelEnableMask(handle[iBrd], &fChMask[iBrd]);
    PrintError(err, "GetChannelEnableMask");

    err = CAEN_DGTZ_GetInfo(fHandler[iBrd], &info);
    PrintError(err, "GetInfo");

    fNChs = info.Channels;

    std::cout << "Model name:\t" << info.ModelName << "\n"
              << "Model number:\t" << info.Model << "\n"
              << "No. channels:\t" << info.Channels << "\n"
              << "Format factor:\t" << info.FormFactor << "\n"
              << "Family code:\t" << info.FamilyCode << "\n"
              << "Firmware revision of the FPGA on the mother board (ROC):\t"
              << info.ROC_FirmwareRel << "\n"
              << "Firmware revision of the FPGA on the daughter board (AMC):\t"
              << info.AMC_FirmwareRel << "\n"
              << "Serial number:\t" << info.SerialNumber << "\n"
              << "PCB revision:\t" << info.PCB_Revision << "\n"
              << "No. bits of the ADC:\t" << info.ADC_NBits << "\n"
              << "Device handler of CAENComm:\t" << info.CommHandle << "\n"
              << "Device handler of CAENVME:\t" << info.VMEHandle << "\n"
              << "License number:\t" << info.License << std::endl;

    // Copy from digites
    if (info.FamilyCode == 5) {
      fDigitizerModel = 751;
      fTSample[iBrd] = 1;
      fNBits = 10;
    } else if (info.FamilyCode == 7) {
      fDigitizerModel = 780;
      fTSample[iBrd] = 10;
      fNBits = 14;
    } else if (info.FamilyCode == 13) {
      fDigitizerModel = 781;
      fTSample[iBrd] = 10;
      fNBits = 14;
    } else if (info.FamilyCode == 0) {
      fDigitizerModel = 724;
      fTSample[iBrd] = 10;
      fNBits = 14;
    } else if (info.FamilyCode == 11) {
      fDigitizerModel = 730;
      fTSample[iBrd] = 2;
      fNBits = 14;
    } else if (info.FamilyCode == 14) {
      fDigitizerModel = 725;
      fTSample[iBrd] = 4;
      fNBits = 14;
    } else if (info.FamilyCode == 3) {
      fDigitizerModel = 720;
      fTSample[iBrd] = 4;
      fNBits = 12;
    } else if (info.FamilyCode == 999) {  // temporary code for Hexagon
      fDigitizerModel = 5000;
      fTSample[iBrd] = 10;
      fNBits = 14;
    } else {
      PrintError(err, "Check Family code @ GetBoardInfo");
    }

    uint32_t majorNumber = atoi(info.AMC_FirmwareRel);
    if (fDigitizerModel == 5000) {        // Hexagon
      fFirmware = FirmWareCode::DPP_PHA;  // It will be never used at ELI?
    } else if (majorNumber == 128) {
      fFirmware = FirmWareCode::DPP_PHA;  // It will be never used at ELI?
    } else if (majorNumber == 130) {
      fFirmware = FirmWareCode::DPP_ChInt;
    } else if (majorNumber == 131) {
      fFirmware = FirmWareCode::DPP_PSD;
    } else if (majorNumber == 132) {
      fFirmware = FirmWareCode::DPP_PSD;
    } else if (majorNumber == 136) {
      fFirmware = FirmWareCode::DPP_PSD;  // NOTE: valid also for x725
    } else if (majorNumber == 139) {
      fFirmware = FirmWareCode::DPP_PHA;  // NOTE: valid also for x725
    } else {
      fFirmware = FirmWareCode::STD;
    }

    std::cout << "Time sample length:\t" << fTSample << " ns\n"
              << "ADC resolution:\t" << fNBits << " bits\n"
              << "Firmware code:\t" << int(fFirmware) << std::endl;
  }
}

void TDigiTes::Start() { StartAcquisition(fWDcfg); }

void TDigiTes::Stop() { StopAcquisition(fWDcfg); }

void TDigiTes::SendSWTrigger()
{
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    auto err = CAEN_DGTZ_SendSWtrigger(fHandler[b]);
    PrintError(err, "SendSWTrigger");
  }
}

void TDigiTes::PrintError(const CAEN_DGTZ_ErrorCode &err,
                          const std::string &funcName) const
{
  // if (true) {  // 0 is success
  if (err < 0) {  // 0 is success
    std::cout << "In " << funcName << ", error code = " << err << ", "
              << ErrorCodeMap[err] << std::endl;
    // CAEN_DGTZ_CloseDigitizer(fHandler);
    // throw err;
  }
}
