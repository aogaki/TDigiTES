#ifndef TPSDData_hpp
#define TPSDData_hpp 1

#include <CAENDigitizer.h>
#include <CAENDigitizerType.h>

typedef struct {
  unsigned char ModNumber;
  unsigned char ChNumber;
  uint64_t TimeStamp;
<<<<<<< HEAD
=======
  uint32_t RecordLength;
  uint16_t *WaveForm;
  // uint16_t WaveForm[1024]; // Check maximum number of samples
>>>>>>> d028cf2f65b9860f397188a26dfa879dac337531
  int16_t ChargeShort;
  int16_t ChargeLong;
  uint32_t RecordLength;
  uint16_t *WaveForm;
} TPSDData_t;

#endif
