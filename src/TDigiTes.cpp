#include "TDigiTes.hpp"

#include <CAENDigitizer.h>

#include <iostream>
#include <string>

#include "ErrorCodeMap.hpp"

TDigiTes::TDigiTes(){};

TDigiTes::~TDigiTes(){};

void TDigiTes::LoadParameters(std::string fileName)
{
  // auto configFileName = "digiTES_Config.txt";
  std::cout << "Open the input file " << fileName << std::endl;
  FILE *f_ini = fopen(fileName.c_str(), "r");

  if (f_ini == nullptr) {
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
    CAEN_DGTZ_Reset(fHandler[b]);

    // std::cout << fHandler[b] << "\t" << handle[b] << std::endl;
    // handle[b] = fHandler[b];

    char cstr[500];
    if (ReadBoardInfo(b, cstr, fWDcfg, fHandler) < 0) {
      std::cout << "ReadBoardInfo failed" << std::endl;
    }
    // std::cout << "NumAcqCh\t" << fWDcfg.NumAcqCh << std::endl;
    // std::cout << "NumPhyCh\t" << fWDcfg.NumPhyCh << std::endl;
    // std::cout << "MAX_NCH\t" << MAX_NCH << std::endl;
    // for (auto i = fWDcfg.NumAcqCh; i < MAX_NCH; i++)
    for (auto i = fWDcfg.NumPhyCh; i < MAX_NCH; i++)
      fWDcfg.EnableInput[b][i] = 0;
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
    ret = (CAEN_DGTZ_ErrorCode)ProgramDigitizer(b, fWDcfg, fHandler);
    if (ret != CAEN_DGTZ_Success) {
      std::cout << "ERROR: Failed to program the digitizer" << std::endl;
      exit(1);
    }
    SetVirtualProbes(b, fWDcfg, fHandler);
  }

  GetBoardInfo();
}

void TDigiTes::GetBoardInfo()
{
  CAEN_DGTZ_BoardInfo_t info;
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++) {
    auto err = CAEN_DGTZ_GetInfo(fHandler[iBrd], &info);
    PrintError(err, "GetInfo");

    fNChs[iBrd] = info.Channels;

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
    } else if (info.FamilyCode == 4) {
      fDigitizerModel = 740;
      fTSample[iBrd] = 16;
      fNBits = 12;
    } else if (info.FamilyCode == 999) {  // temporary code for Hexagon
      fDigitizerModel = 5000;
      fTSample[iBrd] = 10;
      fNBits = 14;
    } else {
      PrintError(err, "Check Family code @ GetBoardInfo");
    }

    auto previousFW = fFirmware;
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
    } else if (majorNumber == 135) {
      fFirmware = FirmWareCode::DPP_QDC;
    } else if (majorNumber == 136) {
      fFirmware = FirmWareCode::DPP_PSD;  // NOTE: valid also for x725
    } else if (majorNumber == 139) {
      fFirmware = FirmWareCode::DPP_PHA;  // NOTE: valid also for x725
    } else {
      fFirmware = FirmWareCode::STD;
    }
    if (iBrd != 0 && previousFW != fFirmware) {
      std::cerr
          << "ERROR: Different firmware code in the same digitizer group.\n"
             "Please make sure that all digitizers have the same firmware.\n"
             "Or use different digitizer group for different firmware."
          << std::endl;
      exit(1);
    }

    // Check ch mask
    if (fFirmware == FirmWareCode::DPP_QDC) {
      err = CAEN_DGTZ_GetGroupEnableMask(fHandler[iBrd], &fChMask[iBrd]);
      PrintError(err, "GetChannelEnableMask");
    } else {
      err = CAEN_DGTZ_GetChannelEnableMask(fHandler[iBrd], &fChMask[iBrd]);
      PrintError(err, "GetChannelEnableMask");
    }

    std::cout << "Time sample length:\t" << fTSample[iBrd] << " ns\n"
              << "ADC resolution:\t" << fNBits << " bits\n"
              << "Firmware code:\t" << int(fFirmware) << std::endl;
  }

  std::cout << std::endl;
}

void TDigiTes::Start()
{
  for (auto iBrd = 0; iBrd < MAX_NBRD; iBrd++) {
    for (auto iCh = 0; iCh < MAX_NCH; iCh++) {
      fPreviousTime[iBrd][iCh] = 0;
      fTimeOffset[iBrd][iCh] = 0;
      fLostTrgCounter[iBrd][iCh] = 0;
      fLostTrgCounterOffset[iBrd][iCh] = 0;
    }
  }

  fDataVec.reset(new std::vector<std::unique_ptr<TreeData_t>>);
  fDataVec->reserve(1000000);

  StartAcquisition(fWDcfg, fHandler);
}

void TDigiTes::Stop() { StopAcquisition(fWDcfg, fHandler); }

void TDigiTes::ReadEvents()
{
  ReadRawData();
  DecodeRawData();
}

void TDigiTes::SetIntervals()
{
  // For usleep
  fReadInterval = 500;
  fDecodeInterval = 500;
}

void TDigiTes::SendSWTrigger()
{
  for (auto b = 0; b < fWDcfg.NumBrd; b++) {
    auto err = CAEN_DGTZ_SendSWtrigger(fHandler[b]);
    PrintError(err, "SendSWTrigger");
  }
}

std::unique_ptr<std::vector<std::unique_ptr<TreeData_t>>> TDigiTes::GetData()
{
  std::lock_guard<std::mutex> lock(fMutex);
  auto retVal = std::move(fDataVec);
  fDataVec.reset(new std::vector<std::unique_ptr<TreeData_t>>);
  fDataVec->reserve(100000);
  return retVal;
}

void TDigiTes::Test()
{
  // for (auto b = 0; b < fWDcfg.NumBrd; b++) {
  //   CAEN_DGTZ_DisableEventAlignedReadout(fHandler[b]);
  // }
}

void TDigiTes::DumpRegisters()
{
  for (auto iBrd = 0; iBrd < fWDcfg.NumBrd; iBrd++)
    SaveRegImage(fHandler[iBrd], fWDcfg);
}

void TDigiTes::PrintError(const CAEN_DGTZ_ErrorCode &err,
                          const std::string &funcName) const
{
  // No try and throw now
  if (err < 0) {  // 0 is success
    std::cout << "In " << funcName << ", error code = " << err << ", "
              << ErrorCodeMap[err] << std::endl;
    // CAEN_DGTZ_CloseDigitizer(fHandler);
    // throw err;
  }
}
