#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Op 23 and event 3: where the string wear check stands, for a phone to guide it.
// The stick measures and judges; WearCheck's enums go out as their numbers.
namespace BleWear {
constexpr size_t BYTES=83,EVENT_BYTES=10;
constexpr uint8_t EVENT_TYPE=3;
struct Values {
  uint8_t active=0,stage=0,step=0,alignment=0,detectorReady=0,outcome=0,mountHint=0,tiltValid=0;
  float tilt=0,lastRpm=0;
  uint8_t count[3]={};
  float rpm[3][3]={};
  float mean=0,medianMean=0,stageMean[3]={};
  // Where the stick's own aiming dot sits, -1 to 1 with the target ring at 0.5.
  float reticleX=0,reticleY=0;
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
  putF(v.reticleX);putF(v.reticleY);
  return size_t(p-out);
}
// What changes the instruction on screen: sent as it changes, so it need not wait for a poll.
inline void encodeEvent(const Values &v,uint8_t *out) {
  out[0]=EVENT_TYPE;out[1]=v.stage;out[2]=v.step;out[3]=v.alignment;out[4]=v.detectorReady;out[5]=v.outcome;out[6]=v.mountHint;
  out[7]=v.count[0];out[8]=v.count[1];out[9]=v.count[2];
}
}
