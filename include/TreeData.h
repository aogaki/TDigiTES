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

  TreeData(uint8_t mod, uint8_t ch, uint64_t ts, double fineTS, uint16_t cl,
           uint16_t cs, uint32_t extras = 0, uint32_t nSamples = 0)
  {
    Mod = mod;
    Ch = ch;
    TimeStamp = ts;
    FineTS = fineTS;
    ChargeLong = cl;
    ChargeShort = cs;
    Extras = extras;
    RecordLength = nSamples;
    Trace1.resize(nSamples);
    Trace2.resize(nSamples);
    DTrace1.resize(nSamples);
    DTrace2.resize(nSamples);
    DTrace3.resize(nSamples);
    DTrace4.resize(nSamples);
  };

  TreeData(const TreeData &td)
  {
    Mod = td.Mod;
    Ch = td.Ch;
    TimeStamp = td.TimeStamp;
    FineTS = td.FineTS;
    ChargeLong = td.ChargeLong;
    ChargeShort = td.ChargeShort;
    Extras = td.Extras;
    RecordLength = td.RecordLength;
    Trace1 = td.Trace1;
    Trace2 = td.Trace2;
    DTrace1 = td.DTrace1;
    DTrace2 = td.DTrace2;
    DTrace3 = td.DTrace3;
    DTrace4 = td.DTrace4;
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
  std::vector<int16_t> Trace1;
  std::vector<int16_t> Trace2;
  std::vector<int8_t> DTrace1;
  std::vector<int8_t> DTrace2;
  std::vector<int8_t> DTrace3;
  std::vector<int8_t> DTrace4;

  static const uint16_t OneHitSize =
      sizeof(Mod) + sizeof(Ch) + sizeof(TimeStamp) + sizeof(FineTS) +
      sizeof(ChargeLong) + sizeof(ChargeShort) + sizeof(RecordLength);
};
typedef TreeData TreeData_t;

class SmallData
{  // no getter setter.  using public member variables.
 public:
  SmallData(){};

  ~SmallData(){};

  uint8_t Mod;
  uint8_t Ch;
  double FineTS;  // expecting 64 bits
  uint16_t ChargeLong;

  static const uint16_t OneHitSize =
      sizeof(Mod) + sizeof(Ch) + sizeof(FineTS) + sizeof(ChargeLong);
};
typedef SmallData SmallData_t;

#endif
