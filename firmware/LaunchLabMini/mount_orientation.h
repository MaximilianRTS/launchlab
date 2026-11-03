#pragma once
#include <cstdint>

// Normalize IMU samples to the original Top launcher frame before fusion.
// M5Unified native axes: +X screen-right, +Y screen-top, +Z screen-normal.
// The supported sideways Sidecar pose has B key / +X toward the user and
// Hat / +Y toward the ripcord. Its +X is Top +Z; its +Z is Top -X.
// Top 90 L / R are Top turned 90 degrees in the launcher's top plane, counter-
// clockwise and clockwise seen from above. L: device +X lies along launcher +Y
// and device +Y along launcher -X. R: device +X along -Y and device +Y along +X.
// These proper rotations apply equally to acceleration and angular rate.
// LCD Flip is a separate presentation choice and never changes this frame.
namespace MountOrientation {
enum class Mode:uint8_t { Top=0, Sidecar=1, Top90L=2, Top90R=3 };
static constexpr unsigned MODES=4;
// A Top-family mounting's turn on the launcher, seen from above: +1 clockwise.
inline int quarterTurns(Mode mode){return mode==Mode::Top90R?1:mode==Mode::Top90L?-1:0;}
inline bool valid(unsigned value){return value<MODES;}
inline const char *label(Mode mode) {
  switch(mode) {
    case Mode::Sidecar:return "Sidecar";
    case Mode::Top90L:return "Top 90 L";
    case Mode::Top90R:return "Top 90 R";
    default:return "Top";
  }
}
inline void mapVector(Mode mode,const float *in,float *out) {
  // Cache every component so in-place mapping has the same result.
  const float x=in[0],y=in[1],z=in[2];
  switch(mode) {
    case Mode::Sidecar:out[0]=-z;out[1]=y;out[2]=x;break;
    case Mode::Top90L:out[0]=-y;out[1]=x;out[2]=z;break;
    case Mode::Top90R:out[0]=y;out[1]=-x;out[2]=z;break;
    default:out[0]=x;out[1]=y;out[2]=z;break;
  }
}
// The inverse of mapVector: the device vector that Mounting `mode` maps to `in`.
inline void unmapVector(Mode mode,const float *in,float *out) {
  const float x=in[0],y=in[1],z=in[2];
  switch(mode) {
    case Mode::Sidecar:out[0]=z;out[1]=y;out[2]=-x;break;
    case Mode::Top90L:out[0]=y;out[1]=-x;out[2]=z;break;
    case Mode::Top90R:out[0]=-y;out[1]=x;out[2]=z;break;
    default:out[0]=x;out[1]=y;out[2]=z;break;
  }
}
// The launcher-frame vector `in`, measured in Mounting `from`, as Mounting `to`
// would have read the same device sample.
inline void remapVector(Mode from,Mode to,const float *in,float *out) {
  float device[3];unmapVector(from,in,device);
  mapVector(to,device,out);
}
// The Mounting Wear check's hint compares with. A rotated Top has Top's level
// pose (the turn is about the vertical), so level can only tell the Top family
// from Sidecar, never which turn of Top: the hint never suggests one.
inline Mode levelAlternative(Mode mode){return mode==Mode::Sidecar?Mode::Top:Mode::Sidecar;}
// Wear check's captions: the Mounting in use, and what its hint asks.
inline const char *mountCaption(Mode mode) {
  switch(mode) {
    case Mode::Sidecar:return "Sidecar mount";
    case Mode::Top90L:return "Top 90 L mount";
    case Mode::Top90R:return "Top 90 R mount";
    default:return "Top mount";
  }
}
inline const char *hintCaption(Mode mode){return mode==Mode::Sidecar?"Top install?":"Sidecar install?";}
}
