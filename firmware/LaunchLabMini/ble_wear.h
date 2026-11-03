#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Op 23: where the string wear check stands, for a phone to guide it. The stick
// measures and judges; WearCheck's enums go out as their numbers.
namespace BleWear {
constexpr size_t BYTES=75;
struct Values {
  uint8_t active=0,stage=0,step=0,alignment=0,detectorReady=0,outcome=0,mountHint=0,tiltValid=0;
  float tilt=0,lastRpm=0;
  uint8_t count[3]={};
  float rpm[3][3]={};
  float mean=0,medianMean=0,stageMean[3]={};
};
inline size_t encode(const Values &v,uint8_t *out) {
  uint8_t *p=out;
  auto putF=[&p](float x){memcpy(p,&x,4);p+=4;};
  *p++=v.active;*p++=v.stage;*p++=v.step;*p++=v.alignment;*p++=v.detectorReady;*p++=v.outcome;*p++=v.mountHint;*p++=v.tiltValid;
  putF(v.tilt);putF(v.lastRpm);
  for(uint8_t c:v.count)*p++=c;
  for(const auto &stage:v.rpm)for(float r:stage)putF(r);
  putF(v.mean);putF(v.medianMean);
  for(float m:v.stageMean)putF(m);
  return size_t(p-out);
}
}
