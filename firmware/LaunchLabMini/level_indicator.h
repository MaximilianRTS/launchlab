#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "display_orientation.h"

// The stick's level on this launcher (BLE op 14): the stored up direction, as the
// shortest rotation taking it to +Z. Only what is shown passes through it: never
// the IMU, the fusion or RPM. It belongs to one Mounting; changing that clears it.
struct LevelReference {
  static constexpr float LIMIT_COS=.93969262f; // 20 degrees: beyond is the wrong Mounting.
  bool active=false;
  float up[3]={0,0,1},w=1,x=0,y=0;
  uint32_t generation=0;
  void clear(){active=false;w=1;x=y=0;up[0]=up[1]=0;up[2]=1;++generation;}
  bool set(const float *direction) {
    const float n=std::sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if(!(n>0.5f) || !std::isfinite(n) || direction[2]/n<LIMIT_COS)return false;
    for(unsigned i=0;i<3;++i)up[i]=direction[i]/n;
    const float a=1+up[2],b=up[1],c=-up[0],m=std::sqrt(a*a+b*b+c*c);
    w=a/m;x=b/m;y=c/m;active=true;++generation;return true;
  }
  // A launcher-axes vector, as the level launcher would read it.
  void apply(const float *in,float *out)const {
    if(!active){out[0]=in[0];out[1]=in[1];out[2]=in[2];return;}
    const float t[3]={2*y*in[2],-2*x*in[2],2*(x*in[1]-y*in[0])};
    const float v0=in[0],v1=in[1],v2=in[2];
    out[0]=v0+w*t[0]+y*t[2];
    out[1]=v1+w*t[1]-x*t[2];
    out[2]=v2+w*t[2]+x*t[1]-y*t[0];
  }
  // Flash: magic, the Mounting it belongs to, up, check byte. Saved through the
  // settings save, so never in a pull; absent when clear; not in backups.
  static constexpr const char *KEY="level_ref";
  template<class Prefs> bool save(Prefs &prefs,uint8_t mount)const {
    if(!active)return !prefs.isKey(KEY) || prefs.remove(KEY);
    uint8_t bytes[RECORD]={0x4c,mount};
    std::memcpy(bytes+2,up,sizeof up);bytes[RECORD-1]=check(bytes);
    return prefs.putBytes(KEY,bytes,RECORD)==RECORD;
  }
  // A record for another Mounting, or a damaged one, reads as no reference.
  template<class Prefs> void load(Prefs &prefs,uint8_t mount) {
    uint8_t bytes[RECORD];float stored[3];
    if(!prefs.isKey(KEY) || prefs.getBytesLength(KEY)!=RECORD || prefs.getBytes(KEY,bytes,RECORD)!=RECORD ||
       bytes[0]!=0x4c || bytes[1]!=mount || bytes[RECORD-1]!=check(bytes))return;
    std::memcpy(stored,bytes+2,sizeof stored);set(stored);
  }
private:
  static constexpr size_t RECORD=2+sizeof(float)*3+1;
  static uint8_t check(const uint8_t *b){uint8_t c=0;for(size_t i=0;i+1<RECORD;++i)c=uint8_t(c*31+b[i]);return c;}
};
inline LevelReference levelReference;

// Gravity-based screen-plane level, fed only by fresh accelerometer samples.
// This is an aiming aid while still, not a launch-angle measurement.
class StickS3Level {
public:
  static constexpr uint32_t STALE_MS=250;
  static constexpr float RIM_DEGREES=30.0f;
  enum class State:uint8_t { Unavailable, Moving, Valid };
  struct Reading {
    State state=State::Unavailable;
    float degrees=0,right=0,down=0;
    bool valid() const{return state==State::Valid;}
    const char *name() const {
      switch(state){case State::Valid:return "valid";
        case State::Moving:return "moving";default:return "unavailable";}
    }
  };
  // `quarterTurns`: how the stick lies on the launcher, seen from above, +1 for
  // clockwise. Gravity is read in launcher axes, but the dot lives on the stick's
  // screen, so it turns with the stick: a side of the launcher that drops still
  // moves the dot toward that side of the glass.
  static Reading forDisplay(Reading reading,bool flipEnabled,int quarterTurns=0) {
    const float right=reading.right,down=reading.down;
    if(quarterTurns>0){reading.right=down;reading.down=-right;}
    else if(quarterTurns<0){reading.right=-down;reading.down=right;}
    const auto sign=DisplayOrientation::axisSign(flipEnabled);
    reading.right*=sign;reading.down*=sign;
    return reading;
  }
  void feed(float x,float y,float z,uint32_t now) {
    ++samples_;last_[0]=x;last_[1]=y;last_[2]=z;
    const uint32_t elapsed=now-lastSample_;
    lastSample_=now;seen_=true;
    const float length=std::sqrt(x*x+y*y+z*z);
    // Freefall, large acceleration and non-finite data cannot establish level.
    if(!std::isfinite(length) || length<0.75f || length>1.25f) {
      ++rejected_;usable_=initialized_=false;return;
    }
    const float alpha=(!initialized_ || elapsed>STALE_MS)?1.0f:
      float(elapsed)/(200.0f+elapsed);
    const float sample[3]={x,y,z};
    for(unsigned i=0;i<3;++i)gravity_[i]+=alpha*(sample[i]-gravity_[i]);
    initialized_=usable_=true;
  }
  Reading reading(uint32_t now) const {
    return reading(now,[](const float *in,float *out){levelReference.apply(in,out);});
  }
  // The reading of the same gravity carried into another frame by `remap`
  // (Wear check asks how the other Mounting would read the unit).
  template<class Remap>
  Reading reading(uint32_t now,Remap remap) const {
    if(!seen_ || now-lastSample_>STALE_MS)return {};
    if(!usable_)return {State::Moving};
    float g[3];remap(gravity_,g);
    const float horizontal=std::hypot(g[0],g[1]);
    if(std::hypot(horizontal,g[2])<0.5f)return {State::Moving};
    const float angle=std::atan2(horizontal,std::fabs(g[2]))*57.2957795f;
    const float radius=std::min(angle/RIM_DEGREES,1.0f);
    const float scale=horizontal>0.0001f?radius/horizontal:0;
    // Native portrait (rotation 0), USB-C down. The BMI270 diagram has raw
    // +X toward USB and +Y toward screen-right. M5Unified maps these to
    // board +X toward screen-right and +Y toward screen-top (X=rawY,Y=-rawX).
    // Reverse both screen directions per the user's mounted-device check.
    // The angle and rim scale stay unchanged.
    return {State::Valid,angle,g[0]*scale,-g[1]*scale};
  }
  // The filtered up direction before any reference, when the reading is valid.
  bool rawUp(uint32_t now,float *out) const {
    if(!reading(now).valid())return false;
    const float n=std::sqrt(gravity_[0]*gravity_[0]+gravity_[1]*gravity_[1]+gravity_[2]*gravity_[2]);
    for(unsigned i=0;i<3;++i)out[i]=gravity_[i]/n;
    return true;
  }
  uint32_t samples() const{return samples_;}
  uint32_t rejected() const{return rejected_;}
  const float *lastAccel() const{return last_;}
private:
  float gravity_[3]={},last_[3]={};
  uint32_t lastSample_=0,samples_=0,rejected_=0;
  bool seen_=false,usable_=false,initialized_=false;
};
