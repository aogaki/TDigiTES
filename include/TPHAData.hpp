#ifndef TPHAData_hpp
#define TPHAData_hpp 1

#include <CAENDigitizerType.h>

#include <iostream>

class PHAData
{  // no getter setter.  using public member variables.
 public:
  PHAData()
      : Trace1(nullptr), Trace2(nullptr), DTrace1(nullptr), DTrace2(nullptr){};

  PHAData(uint32_t nSamples)
  {
    RecordLength = nSamples;
    Trace1 = new uint16_t[nSamples];
    Trace2 = new uint16_t[nSamples];
    DTrace1 = new uint8_t[nSamples];
    DTrace2 = new uint8_t[nSamples];
  };

  ~PHAData()
  {
    delete[] Trace1;
    Trace1 = nullptr;
    delete[] Trace2;
    Trace2 = nullptr;
    delete[] DTrace1;
    DTrace1 = nullptr;
    delete[] DTrace2;
    DTrace2 = nullptr;
  };

  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint16_t FineTS;
  uint16_t Energy;
  uint32_t Extras;
  uint32_t RecordLength;
  uint16_t *Trace1;
  uint16_t *Trace2;
  uint8_t *DTrace1;
  uint8_t *DTrace2;
};
typedef PHAData PHAData_t;

class NoTracePHAData
{  // no getter setter.  using public member variables.
 public:
  NoTracePHAData(){};

  NoTracePHAData(uint32_t nSamples){};

  ~NoTracePHAData(){};

  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint16_t Energy;
};
typedef NoTracePHAData NoTracePHAData_t;

#endif
