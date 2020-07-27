#ifndef TPSDData_hpp
#define TPSDData_hpp 1

#include <CAENDigitizerType.h>

#include <iostream>

class TPSDData { // no getter setter.  using public member variables.
public:
  TPSDData()
      : Trace1(nullptr), Trace2(nullptr), DTrace1(nullptr), DTrace2(nullptr),
        DTrace3(nullptr), DTrace4(nullptr){};

  TPSDData(uint32_t nSamples) {
    RecordLength = nSamples;
    Trace1 = new uint16_t[nSamples];
    Trace2 = new uint16_t[nSamples];
    DTrace1 = new uint8_t[nSamples];
    DTrace2 = new uint8_t[nSamples];
    DTrace3 = new uint8_t[nSamples];
    DTrace4 = new uint8_t[nSamples];
  };

  ~TPSDData() {
    delete[] Trace1;
    Trace1 = nullptr;
    delete[] Trace2;
    Trace2 = nullptr;
    delete[] DTrace1;
    DTrace1 = nullptr;
    delete[] DTrace2;
    DTrace2 = nullptr;
    delete[] DTrace3;
    DTrace3 = nullptr;
    delete[] DTrace4;
    DTrace4 = nullptr;
  };

  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint16_t FineTS;
  int16_t ChargeShort;
  int16_t ChargeLong;
  uint32_t RecordLength;
  uint32_t Extras;
  uint16_t *Trace1;
  uint16_t *Trace2;
  uint8_t *DTrace1;
  uint8_t *DTrace2;
  uint8_t *DTrace3;
  uint8_t *DTrace4;
};
typedef TPSDData TPSDData_t;

#endif
