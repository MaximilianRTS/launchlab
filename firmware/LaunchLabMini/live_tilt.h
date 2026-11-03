#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "motion_replay.h"

// Live tilt (docs/BLE_PROTOCOL.md): the latest fused pose, the replay's fusion, as
// a BLE event and on the device's page.
namespace LiveTilt {
static constexpr uint32_t PERIOD_MS=100;
static constexpr size_t EVENT_BYTES=9;
struct Gate {
  uint32_t last=0;
  // Nothing during a pull or while the fusion is not ready.
  bool due(uint32_t now,bool launch,bool ready) {
    if(launch || !ready || now-last<PERIOD_MS)return false;
    last=now;return true;
  }
};
inline void encode(const LaunchMotion::Quaternion &q,uint8_t *out) {
  const float parts[4]={q.w,q.x,q.y,q.z};
  out[0]=2;
  for(unsigned j=0;j<4;++j) {
    const auto v=uint16_t(int16_t(std::lround(std::max(-1.0f,std::min(1.0f,parts[j]))*16384)));
    out[1+2*j]=uint8_t(v);out[2+2*j]=uint8_t(v>>8);
  }
}
// The page's pose. Yaw has no compass, so one heading reference, the replay's rule,
// is taken from the first ready pose each time the page opens; after it the model
// turns with the launcher. When the fusion stops, the last pose stays, not live.
struct View {
  uint8_t opened=0;
  bool referenced=false,live=false;
  LaunchMotion::Quaternion heading,shown;
  void update(uint8_t openedNow,bool ready,const LaunchMotion::Quaternion &pose) {
    if(opened!=openedNow){opened=openedNow;referenced=false;}
    live=ready;
    if(!ready)return;
    if(!referenced){heading=MotionReplay::headingFor(pose);referenced=true;}
    shown=LaunchMotion::multiply(heading,pose);
  }
  const LaunchMotion::Quaternion *pose()const{return referenced?&shown:nullptr;}
};
}
