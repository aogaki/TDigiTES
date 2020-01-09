#ifndef TPSDData_hpp
#define TPSDData_hpp 1

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

typedef struct {
  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
  uint32_t RecordLength;
  uint16_t *WaveForm;
  int16_t ChargeShort;
  int16_t ChargeLong;
} TPSDData_t;

#endif
