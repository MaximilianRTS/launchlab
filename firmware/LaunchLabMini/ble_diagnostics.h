#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Op 16: how the optical signal looks, for a phone. Read only, from values the
// acquisition already publishes, so asking does not touch the measurement.
namespace BleDiagnostics {
constexpr size_t BYTES=39;
struct Values {
  uint8_t state=0;
  float minMark=0,lastMark=0,medianMark=0,noise=0,samplesPerSecond=0;
  uint32_t signalMin=0,signalMax=0;
  uint32_t weak=0,slow=0,shape=0,inconsistent=0,dataLoss=0,maxReadGapUs=0;
};
// ADC counts and rates fit 16 bits; anything else, NaN included, is held to the range.
inline uint16_t rounded16(float v){return !(v>0)?0:v>=65535.0f?65535:uint16_t(v+0.5f);}
inline uint16_t clamped16(uint32_t v){return v>65535u?65535:uint16_t(v);}
inline size_t encode(const Values &v,uint8_t *out) {
  uint8_t *p=out;
  auto put16=[&p](uint16_t x){memcpy(p,&x,2);p+=2;};
  auto put32=[&p](uint32_t x){memcpy(p,&x,4);p+=4;};
  *p++=v.state;
  put16(rounded16(v.minMark));put16(rounded16(v.lastMark));put16(rounded16(v.medianMark));put16(rounded16(v.noise));
  put16(clamped16(v.signalMin));put16(clamped16(v.signalMax));put16(rounded16(v.samplesPerSecond));
  put32(v.weak);put32(v.slow);put32(v.shape);put32(v.inconsistent);put32(v.dataLoss);put32(v.maxReadGapUs);
  return size_t(p-out);
}
}
