#pragma once
#include "launch_motion.h"
#include "display_orientation.h"
#include "mount_orientation.h"

// Fixed-center orientation replay. Coordinates below are display units only.
// Translation, height change, distance, velocity and compass heading are absent.
namespace MotionReplay {
// Replay the full stored context at 1x, including real pre/post optical motion.
// Short optical intervals alone can be briefer than one or two display frames.
inline uint32_t playbackDurationMs(const LaunchMotion::Trace &t) {
  if(!t.valid() || !t.fused)return 0;
  const int span=int(t.points[t.count-1].ms)-int(t.points[0].ms);
  return span>0?uint32_t(span):0;
}
inline float playbackProgress(const LaunchMotion::Trace &t,uint32_t elapsedMs) {
  const auto duration=playbackDurationMs(t);
  return duration?std::min(elapsedMs,duration)/float(duration):1.0f;
}
inline bool playbackNeedsFrame(const LaunchMotion::Trace &t,uint32_t elapsedMs,uint32_t lastElapsedMs) {
  // Draw once after crossing the end, then hold that endpoint. Comparing the
  // previous draw also catches a stalled UI that skips over the last sample.
  const auto duration=playbackDurationMs(t);
  return duration && (elapsedMs<duration || lastElapsedMs<duration);
}
// The moment of the recording a playback progress of 0..1 shows.
inline int sourceTime(const LaunchMotion::Trace &t,float progress) {
  return t.points[0].ms+lroundf(progress*playbackDurationMs(t));
}
struct Vec {
  float x=0,y=0,z=0;
  Vec operator+(Vec b)const{return {x+b.x,y+b.y,z+b.z};}
  Vec operator-(Vec b)const{return {x-b.x,y-b.y,z-b.z};}
  Vec operator*(float f)const{return {x*f,y*f,z*f};}
  float length()const{return std::sqrt(x*x+y*y+z*z);}
};
inline Vec rotate(const LaunchMotion::Quaternion &q,Vec v) {
  const float in[3]={v.x,v.y,v.z};float out[3];q.rotate(in,out);return {out[0],out[1],out[2]};
}
inline LaunchMotion::Quaternion forDisplay(LaunchMotion::Quaternion pose,bool flipEnabled) {
  // Change both board and horizontal world axes by 180 degrees about +Z:
  // Rz(pi) * pose * inverse(Rz(pi)). This reverses screen-right/top tilt like
  // the level dots, preserves gravity-up and yaw, and keeps neutral heading.
  // Apply after cache lookup so a display preview also works on saved traces.
  const auto sign=DisplayOrientation::axisSign(flipEnabled);
  pose.x*=sign;pose.y*=sign;
  return pose;
}
// The yaw that starts a pose's barrel (+X) at 45 degrees in the horizontal plane,
// which the protocol's view turns back. Only compass heading: roll and pitch stay.
inline LaunchMotion::Quaternion headingFor(const LaunchMotion::Quaternion &pose) {
  const Vec width=rotate(pose,{1,0,0});float yaw;
  if(std::hypot(width.x,width.y)>.2f)yaw=-std::atan2(width.y,width.x);
  else {const Vec top=rotate(pose,{0,1,0});yaw=std::atan2(top.x,top.y);}
  yaw+=.785398163f;
  return {std::cos(yaw*.5f),0,0,std::sin(yaw*.5f)};
}
// What counts as straight on this screen: the launcher's direction when the fusion
// first settled, or when Live tilt last opened. While the fusion runs on, a recap
// keeps it, so a launcher turned before a pull shows turned. The phone's replay never
// uses it: there every replay starts at 45 degrees, as the protocol says.
struct KeptHeading {
  bool set=false;
  LaunchMotion::Quaternion q;
  uint32_t fusionResets=0,generation=0;
};
inline KeptHeading keptHeading;
struct Replay {
  static constexpr unsigned COUNT=48;
  bool valid=false;
  LaunchMotion::Quaternion pose[COUNT];int ms[COUNT]={};
  LaunchMotion::Quaternion at(int timeMs)const {
    if(timeMs<=ms[0])return pose[0];
    for(unsigned i=1;i<COUNT;++i)if(timeMs<=ms[i])
      return LaunchMotion::Quaternion::blend(pose[i-1],pose[i],float(timeMs-ms[i-1])/std::max(1,ms[i]-ms[i-1]));
    return pose[COUNT-1];
  }
  bool build(const LaunchMotion::Trace &t,const LaunchMotion::Quaternion *kept=nullptr) {
    valid=false;if(!t.valid() || !t.fused)return false;
    LaunchMotion::Interpolated onset;if(!LaunchMotion::interpolate(t,0,onset))return false;
    // Reset only compass heading. Preserve real gravity-relative roll/pitch:
    // inverse(onset)*pose would erase initial tilt and show a fictitious flat
    // start. Start the barrel (+X) at 45 degrees in the horizontal plane; the
    // protocol's view turns that back, so it comes toward the viewer.
    const auto heading=kept?*kept:headingFor(LaunchMotion::levelled(onset.q));
    // Include all available stored pre/post context (new captures retain up to
    // 500 ms before and 150 ms after; older stored traces retain their span).
    // Optical onset fixes the relative orientation, not a guessed velocity.
    for(unsigned i=0;i<COUNT;++i) {
      ms[i]=t.points[0].ms+(t.points[t.count-1].ms-t.points[0].ms)*int(i)/int(COUNT-1);
      LaunchMotion::Interpolated sample;if(!LaunchMotion::interpolate(t,ms[i],sample))return false;
      pose[i]=LaunchMotion::multiply(heading,LaunchMotion::levelled(sample.q));
    }
    valid=true;return true;
  }
};
// The reference frame the scene's scale is fitted to. Fixed for every capture,
// and larger than the launcher's reach, so the model fits any pose. Not meters.
struct Cube {
  Vec low{-.84f,-.84f,-.84f},high{.84f,.84f,.84f};
  Vec corner(unsigned i)const {
    static constexpr unsigned bits[8]={0,1,3,2,4,5,7,6};const unsigned b=bits[i];
    return {b&1?high.x:low.x,b&2?high.y:low.y,b&4?high.z:low.z};
  }
};
// The protocol's View: turn by -45 degrees about Z, undoing the replay's heading,
// to (u,v,z), then look from 40 degrees above the floor and 30 to the side: a
// posed barrel (+u) comes toward the viewer and to the left, the ripcord handle
// (+v) goes right. An orthographic projection: a
// vector's image is never longer than the vector.
inline float viewU(Vec v){return (v.x+v.y)*.70710678f;}
inline float viewV(Vec v){return (v.y-v.x)*.70710678f;}
inline Vec camera(Vec v) {
  const float u=viewU(v),w=viewV(v);
  return {w*.866f-u*.5f,(u*.866f+w*.5f)*.643f-v.z*.766f,0};
}
// How far the ripcord is out at `ms` on the replay's clock, as a fraction of the turns
// up to the fastest: the cord has spun that much of itself. 1 is the fastest turn, when
// the ripcord has left. A curve point's time is its ms minus the curve's lead. Without
// a rise to follow, the ripcord just stays.
template<class Curve> inline float ripcordAt(const Curve &c,int ms) {
  if(c.count<2)return 0;
  unsigned fastest=0;
  for(unsigned i=1;i<c.count;++i)if(c.rpm[i]>c.rpm[fastest])fastest=i;
  if(!fastest)return 0;
  const int lead=c.leadMs;
  if(ms>=int(c.ms[fastest])-lead)return 1;
  unsigned turn=0;
  while(turn<fastest && ms>=int(c.ms[turn])-lead)++turn;
  if(!turn)return 0;
  const int from=int(c.ms[turn-1])-lead,to=int(c.ms[turn])-lead;
  return (float(turn-1)+float(ms-from)/std::max(1,to-from))/fastest;
}

// The Floor: a level ring fixed under the model, as line segments between points
// on a circle (at multiples of 22.5 degrees in any frame turned by 45). A ring
// looks the same whichever way Flip and Mounting turn the launcher above it.
namespace Floor {
static constexpr unsigned SEGMENTS=16;
static constexpr float RADIUS=.95f,HEIGHT=-.32f;
inline Vec point(unsigned i) {
  static constexpr float COSINE[SEGMENTS]={1,.92387953f,.70710678f,.38268343f,0,-.38268343f,-.70710678f,-.92387953f,
    -1,-.92387953f,-.70710678f,-.38268343f,0,.38268343f,.70710678f,.92387953f};
  return {RADIUS*COSINE[i%SEGMENTS],RADIUS*COSINE[(i+12)%SEGMENTS],HEIGHT};
}
}

// The launcher (docs/BLE_PROTOCOL.md, Launcher model): boxes in launcher axes,
// in flash. Nothing is computed or kept between frames.
namespace Launcher {
enum Part:uint8_t {Head,Neck,Barrel,Stem,Bar,Stick,Screen,PARTS};
struct Box {Vec low,high;};
static constexpr float SCALE=.62f;
// How far the handle travels before it leaves, in spec units: it stays on screen.
static constexpr float RIPCORD_TRAVEL=.5f;
static constexpr Vec CENTRE{.6f,-.1f,0};
static constexpr Box BODY[6]={
  {{-.45f,-.45f,-.18f},{.45f,.45f,.15f}},  // Head
  {{.45f,-.36f,-.14f},{.85f,.36f,.10f}},   // Neck
  {{.85f,-.30f,-.16f},{2.10f,.30f,.08f}},  // Barrel
  {{-.05f,.45f,-.05f},{.05f,.78f,.05f}},   // Ripcord stem
  {{-.22f,.78f,-.05f},{.22f,.86f,.05f}}};  // Ripcord bar
// By MountOrientation::Mode: Top, Sidecar, Top 90 L, Top 90 R.
static constexpr Box STICK[MountOrientation::MODES]={
  {{-.20f,-.38f,.15f},{.20f,.38f,.36f}},{{-.66f,-.38f,-.20f},{-.45f,.38f,.20f}},
  {{-.38f,-.43f,.15f},{.38f,-.03f,.36f}},{{-.38f,.03f,.15f},{.38f,.43f,.36f}}};
static constexpr Box SCREEN[MountOrientation::MODES]={
  {{-.13f,-.10f,.36f},{.13f,.28f,.37f}},{{-.67f,-.10f,-.13f},{-.66f,.28f,.13f}},
  {{-.28f,-.36f,.36f},{.10f,-.10f,.37f}},{{-.10f,.10f,.36f},{.28f,.36f,.37f}}};
inline Box box(Part part,MountOrientation::Mode mount) {
  const unsigned m=MountOrientation::valid(unsigned(mount))?unsigned(mount):0;
  if(part==Stick)return STICK[m];
  if(part==Screen)return SCREEN[m];
  return BODY[part];
}
// Spec units to scene units.
inline Vec place(Vec v){return (v-CENTRE)*SCALE;}
// Corners by bit (1 +X, 2 +Y, 4 +Z); faces -X,+X,-Y,+Y,-Z,+Z.
static constexpr uint8_t QUADS[6][4]={{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6},{0,1,3,2},{4,5,7,6}};
static constexpr Vec NORMALS[6]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
// World axes of the launcher as this screen draws it, true to the glass: each part
// points where it really is from the screen as held. From the device axes (+Y pin
// end, +X = Y x Z) and mount_orientation.h's mapping, the screen's top is +Y with
// Flip On and -Y with Flip Off (rotation 2), its right +X and -X. So this is the
// protocol's View, turned about the vertical to put that top at the far end, and by
// a further 30 degrees that takes out the View's side angle, which would skew it:
//   Flip Off: Top 90 R barrel down, handle right; Top 90 L barrel up, handle left;
//   Top and Sidecar barrel left, handle down. Flip On: each turned 180 degrees.
// forDisplay and its sign give the 180 degrees of Flip Off. Gravity is kept.
struct Axes {Vec x,y,z;
  Vec at(Vec v)const{return x*v.x+y*v.y+z*v.z;}
};
inline Axes axes(const LaunchMotion::Quaternion &displayed,bool flipEnabled,
                 MountOrientation::Mode mount=MountOrientation::Mode::Top) {
  const float sign=DisplayOrientation::axisSign(flipEnabled);
  const float pins[3]={0,1,0};float top[3];MountOrientation::mapVector(mount,pins,top);
  // Native screen-top (the stick's pin end) in launcher axes goes to the view's
  // far end, -X in the undone heading: a quarter-turn matrix of it, then the 30.
  auto turned=[&](Vec v) {
    const float x=-top[0]*v.x-top[1]*v.y,y=top[1]*v.x-top[0]*v.y;
    return Vec{x*.8660254f-y*.5f,x*.5f+y*.8660254f,v.z};
  };
  return {turned(rotate(displayed,{sign,0,0})),turned(rotate(displayed,{0,sign,0})),turned(rotate(displayed,{0,0,1}))};
}
// Toward the viewer along the view axis (.663,.383,.643) in (u,v,z).
inline float towardViewer(Vec v){return viewU(v)*.663f+viewV(v)*.383f+v.z*.643f;}
// A lit fraction, 0..1, for a face's world normal: from above, near the viewer.
inline float light(Vec normal){return std::max(0.0f,viewU(normal)*.35f-viewV(normal)*.25f+normal.z*.9f);}
// Visible faces far to near along the view axis, with no sort. Axis planes
// separate the boxes, so which side of each the viewer is on, whatever the view
// axis, gives their depth order: head group / neck / barrel across X, cord / head / stem / bar along Y,
// a Top stick over everything across Z and a Sidecar stick behind the head across
// X. Back-face culling completes each box. The screen always follows the stick.
// emit(part,corners,quad,worldNormal): corners are the box's 8 scene points.
// `pull`: how far the ripcord is out, from ripcordAt. At 1 it and its cord are gone.
template<class Emit> inline void paint(const Axes &a,MountOrientation::Mode mount,Emit &&emit,float pull=0) {
  auto part=[&](Part p) {
    if(pull>=1 && (p==Stem || p==Bar))return;
    Box b=box(p,mount);Vec world[8];
    if(p==Stem)b.high.y+=pull*RIPCORD_TRAVEL;
    if(p==Bar){b.low.y+=pull*RIPCORD_TRAVEL;b.high.y+=pull*RIPCORD_TRAVEL;}
    for(unsigned i=0;i<8;++i)world[i]=a.at(place({i&1?b.high.x:b.low.x,i&2?b.high.y:b.low.y,i&4?b.high.z:b.low.z}));
    for(unsigned f=0;f<6;++f) {
      const Vec n=a.at(NORMALS[f]);
      if(towardViewer(n)>0)emit(p,world,QUADS[f],n);
    }
  };
  const bool fromPlusX=towardViewer(a.x)>=0,fromPlusY=towardViewer(a.y)>=0,fromAbove=towardViewer(a.z)>=0;
  auto headGroup=[&](){
    if(fromPlusY){part(Head);part(Stem);part(Bar);}
    else{part(Bar);part(Stem);part(Head);}
  };
  auto stick=[&](){part(Stick);part(Screen);};
  const bool sidecar=mount==MountOrientation::Mode::Sidecar;
  if(!sidecar && !fromAbove)stick();
  if(fromPlusX) {
    if(sidecar)stick();
    headGroup();part(Neck);part(Barrel);
  } else {
    part(Barrel);part(Neck);headGroup();
    if(sidecar)stick();
  }
  if(!sidecar && fromAbove)stick();
}
}

// The BLE Get replay payload (docs/BLE_PROTOCOL.md, Tilt replay): the poses the
// recap draws, before forDisplay, in the launcher's axes. `scratch` is the
// caller's: a Replay is too large for the loop stack.
static constexpr size_t PHONE_HEADER_BYTES=11,PHONE_POINT_BYTES=12,PHONE_BYTES=PHONE_HEADER_BYTES+Replay::COUNT*PHONE_POINT_BYTES;
inline size_t encodeForPhone(const LaunchMotion::Trace &t,Replay &scratch,uint8_t *out,size_t capacity) {
  if(capacity<PHONE_BYTES)return 0;
  auto put=[&out](uint32_t v,unsigned bytes){for(unsigned i=0;i<bytes;++i)*out++=uint8_t(v>>(8*i));};
  auto unit=[](float v){return uint32_t(int32_t(std::lround(std::max(-1.0f,std::min(1.0f,v))*16384)));};
  // Whatever the recap can draw, it sends. Bit 0 is its Tilt approx. mark.
  const bool usable=scratch.build(t);
  put(t.number,4);put(t.durationMs,4);put(usable?Replay::COUNT:0,2);put(usable && t.approximate()?1:0,1);
  if(!usable)return PHONE_HEADER_BYTES;
  for(unsigned i=0;i<Replay::COUNT;++i) {
    const auto &q=scratch.pose[i];put(uint32_t(scratch.ms[i]),4);
    put(unit(q.w),2);put(unit(q.x),2);put(unit(q.y),2);put(unit(q.z),2);
  }
  return PHONE_BYTES;
}
}
