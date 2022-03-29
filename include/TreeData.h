#ifndef TreeData_hpp
#define TreeData_hpp 1

#include <cstdint>
#include <vector>

class TreeData
{  // no getter setter.  using public member variables.
 public:
  TreeData(){};

  TreeData(uint32_t nSamples)
  {
    RecordLength = nSamples;
    Trace1.resize(nSamples);
    Trace2.resize(nSamples);
    DTrace1.resize(nSamples);
    DTrace2.resize(nSamples);
    DTrace3.resize(nSamples);
    DTrace4.resize(nSamples);
  };

  ~TreeData(){};

  uint8_t Mod;
  uint8_t Ch;
  uint64_t TimeStamp;
  double FineTS;  // expecting 64 bits
  uint16_t ChargeLong;
  uint16_t ChargeShort;
  uint32_t Extras;
  uint32_t RecordLength;
  std::vector<uint16_t> Trace1;
  std::vector<uint16_t> Trace2;
  std::vector<uint8_t> DTrace1;
  std::vector<uint8_t> DTrace2;
  std::vector<uint8_t> DTrace3;
  std::vector<uint8_t> DTrace4;

  static const uint16_t OneHitSize =
      sizeof(Mod) + sizeof(Ch) + sizeof(TimeStamp) + sizeof(FineTS) +
      sizeof(ChargeLong) + sizeof(ChargeShort) + sizeof(RecordLength);
};
typedef TreeData TreeData_t;

#endif
