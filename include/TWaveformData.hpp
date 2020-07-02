#ifndef TWaveformData_hpp
#define TWaveformData_hpp 1

#include <CAENDigitizerType.h>

#include <iostream>

class TWaveformData
{  // no getter setter.  using public member variables.
 public:
  TWaveformData() : Trace1(nullptr){};

  TWaveformData(uint32_t nSamples)
  {
    RecordLength = nSamples;
    Trace1 = new uint16_t[nSamples];
  };

  ~TWaveformData()
  {
    delete[] Trace1;
    Trace1 = nullptr;
  };

  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint32_t RecordLength;
  uint16_t *Trace1;
};
typedef TWaveformData WaveformData_t;

#endif
