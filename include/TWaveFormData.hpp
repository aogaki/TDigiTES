#ifndef TWaveFormData_hpp
#define TWaveFormData_hpp 1

#include <CAENDigitizerType.h>

#include <iostream>

class TWaveFormData
{  // no getter setter.  using public member variables.
 public:
  TWaveFormData() : Trace1(nullptr){};

  TWaveFormData(uint32_t nSamples)
  {
    RecordLength = nSamples;
    Trace1 = new uint16_t[nSamples];
  };

  ~TWaveFormData()
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
typedef TWaveFormData WaveFormData_t;

#endif
