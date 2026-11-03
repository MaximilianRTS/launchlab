#include "Preferences.h"
#include "../LaunchLabMini/live_tilt.h"
#include <string>
static constexpr float DEG_PER_RAD=57.29578f;
#include <cassert>
#include <iostream>
using namespace LiveTilt;
using LaunchMotion::Quaternion;
static bool near(float a,float b,float t=.0005f){return std::fabs(a-b)<t;}
static int16_t i16(const uint8_t *p){return int16_t(uint16_t(p[0])|uint16_t(p[1])<<8);}
int main() {
  // The wire: type 2, then i16 w, x, y, z scaled by 16384.
  uint8_t e[EVENT_BYTES];assert(EVENT_BYTES==9);
  encode(Quaternion{1,0,0,0},e);
  assert(e[0]==2 && i16(e+1)==16384 && i16(e+3)==0 && i16(e+5)==0 && i16(e+7)==0);
  encode(Quaternion{.5f,-.5f,.25f,-.25f},e);
  assert(i16(e+1)==8192 && i16(e+3)==-8192 && i16(e+5)==4096 && i16(e+7)==-4096);
  encode(Quaternion{2,-2,0,0},e);assert(i16(e+1)==16384 && i16(e+3)==-16384);
  // About 10 Hz, never in a pull, and only when the fusion is ready. Refused
  // ticks do not use up the period. Resting does not stop it: the fusion runs.
  Gate g;
  assert(!g.due(99,false,true));
  assert(g.due(100,false,true) && !g.due(150,false,true) && g.due(200,false,true));
  assert(!g.due(1000,true,true) && !g.due(1000,false,false));
  assert(g.last==200 && g.due(1000,false,true) && g.last==1000);
  // headingFor: only yaw, and the barrel starts at 45 degrees in the plane.
  using namespace MotionReplay;
  Quaternion tipped;const float pitch[3]={0,25,0},roll[3]={15,0,0},yaw[3]={0,0,73};
  {Quaternion a,b,c;a.integrate(roll,1);b.integrate(pitch,1);c.integrate(yaw,1);tipped=LaunchMotion::multiply(c,LaunchMotion::multiply(b,a));}
  const auto heading=headingFor(tipped);
  assert(near(heading.x,0) && near(heading.y,0));
  const Vec barrel=rotate(LaunchMotion::multiply(heading,tipped),{1,0,0});
  assert(near(barrel.x,barrel.y,.001f) && barrel.x>0);
  // The page: no pose until the fusion is ready, then one reference for the
  // opening, so tilt is kept and a real turn turns the model.
  View v;Quaternion any;
  v.update(1,false,any);assert(!v.pose() && !v.live);
  v.update(1,true,tipped);assert(v.pose() && v.live);
  const Vec start=rotate(*v.pose(),{1,0,0});assert(near(start.x,start.y,.001f));
  // Up in launcher axes is the pose's, untouched by the reference.
  const Vec upBefore=rotate(LaunchMotion::inverse(tipped),{0,0,1}),upShown=rotate(LaunchMotion::inverse(*v.pose()),{0,0,1});
  assert((upBefore-upShown).length()<.001f);
  Quaternion turn;const float quarter[3]={0,0,90};turn.integrate(quarter,1);
  v.update(1,true,LaunchMotion::multiply(turn,tipped));
  const Vec turned=rotate(*v.pose(),{1,0,0});
  assert(near(turned.x,-start.y,.002f) && near(turned.y,start.x,.002f));
  // When the fusion stops, the last pose stays and is not live.
  const Quaternion held=*v.pose();
  v.update(1,false,any);assert(v.pose() && !v.live && Quaternion::difference(*v.pose(),held)<.01f);
  // A new opening takes a new reference: the barrel is back at 45 degrees.
  v.update(2,true,LaunchMotion::multiply(turn,tipped));
  const Vec again=rotate(*v.pose(),{1,0,0});assert(near(again.x,again.y,.001f) && again.x>0);
  // Reopened while the fusion is not ready: no stale reference is used.
  v.update(3,false,any);v.update(3,true,tipped);
  const Vec fresh=rotate(*v.pose(),{1,0,0});assert(near(fresh.x,fresh.y,.001f));
  // The level reference: flat reads 0, everywhere shown, and nothing below that.
  {
    const float tip=std::sin(9/DEG_PER_RAD),cosine=std::cos(9/DEG_PER_RAD);
    const float rest[3]={tip*.6f,tip*.8f,cosine};
    LevelReference r;assert(r.set(rest) && r.active);
    float out[3];r.apply(rest,out);assert(near(out[0],0) && near(out[1],0) && near(out[2],1));
    // A proper rotation: lengths and angles kept; aliasing safe.
    const float v[3]={.3f,-.5f,.81f};r.apply(v,out);
    assert(near(out[0]*out[0]+out[1]*out[1]+out[2]*out[2],v[0]*v[0]+v[1]*v[1]+v[2]*v[2],.001f));
    float aliased[3]={v[0],v[1],v[2]};r.apply(aliased,aliased);assert(near(aliased[0],out[0]) && near(aliased[2],out[2]));
    // The 20 degree limit, and nonsense, refused without a change.
    LevelReference limit;const float a=20.5f/DEG_PER_RAD,b=19.5f/DEG_PER_RAD;
    const float over[3]={std::sin(a),0,std::cos(a)},under[3]={std::sin(b),0,std::cos(b)},none[3]={0,0,0},down[3]={0,0,-1};
    assert(!limit.set(over) && !limit.active && !limit.set(none) && !limit.set(down) && limit.set(under));
    // Clear: identity again, and a new generation so a cached replay rebuilds.
    const auto generation=limit.generation;limit.clear();assert(!limit.active && limit.generation!=generation);
    limit.apply(v,out);assert(out[0]==v[0] && out[2]==v[2]);
    // The shared one: the level's reading and pose agree, raw stays raw.
    levelReference.clear();
    StickS3Level level;level.feed(rest[0],rest[1],rest[2],1000);
    assert(near(level.reading(1000).degrees,9,.01f));
    float raw[3];assert(level.rawUp(1000,raw) && near(raw[0],rest[0],.001f));
    assert(levelReference.set(raw));
    const auto zero=level.reading(1000);assert(zero.valid() && zero.degrees<.05f && std::hypot(zero.right,zero.down)<.01f);
    assert(level.rawUp(1000,raw) && near(raw[0],rest[0],.001f));
    // The other Mounting's remap still reads the raw gravity.
    const auto rawThere=level.reading(1000,[](const float *in,float *o){o[0]=in[0];o[1]=in[1];o[2]=in[2];});assert(near(rawThere.degrees,9,.01f));
    // A pose whose up is the reference draws flat: the shown pose's up is +Z.
    Quaternion q;
    {const float n=std::sqrt(rest[0]*rest[0]+rest[1]*rest[1]);const float half=9/DEG_PER_RAD/2;
     const Quaternion toUp{std::cos(half),std::sin(half)*rest[1]/n,-std::sin(half)*rest[0]/n,0};q=toUp;}
    const auto shown=LaunchMotion::levelled(q);
    const Vec upShown2=rotate(LaunchMotion::inverse(shown),{0,0,1});
    assert(near(upShown2.x,0,.002f) && near(upShown2.y,0,.002f) && near(upShown2.z,1,.002f));
    // A replay built from that resting pose starts level, its barrel at 45 degrees.
    LaunchMotion::Trace t;t.quality=LaunchMotion::Quality::Valid;t.fused=true;t.rpm=6000;t.durationMs=400;t.count=48;
    for(unsigned i=0;i<t.count;++i){auto &p=t.points[i];p.ms=int16_t(-500+int(i)*25);
      p.q[0]=int16_t(std::lround(q.w*16384));p.q[1]=int16_t(std::lround(q.x*16384));p.q[2]=int16_t(std::lround(q.y*16384));p.q[3]=int16_t(std::lround(q.z*16384));}
    Replay flatReplay;assert(flatReplay.build(t));
    const Vec replayUp=rotate(LaunchMotion::inverse(flatReplay.pose[10]),{0,0,1});
    assert(near(replayUp.x,0,.004f) && near(replayUp.y,0,.004f));
    const Vec replayBarrel=rotate(flatReplay.pose[10],{1,0,0});assert(near(replayBarrel.x,replayBarrel.y,.004f) && replayBarrel.x>0);
    levelReference.clear();
    Replay plain;assert(plain.build(t));
    assert(rotate(LaunchMotion::inverse(plain.pose[10]),{0,0,1}).length()>.9f && std::fabs(rotate(LaunchMotion::inverse(plain.pose[10]),{0,0,1}).x)>.05f);
    // Flash: one Mounting's, round trip; another's or a damaged one reads as none.
    Preferences prefs;prefs.begin("launchlab-mini",false);
    LevelReference saved;assert(saved.set(rest) && saved.save(prefs,3));
    LevelReference again;again.load(prefs,3);assert(again.active && near(again.up[0],saved.up[0]));
    LevelReference other;other.load(prefs,1);assert(!other.active);
    const auto key=std::string("launchlab-mini")+LevelReference::KEY;
    Preferences::data[key][5]^=1;LevelReference damaged;damaged.load(prefs,3);assert(!damaged.active);
    saved.clear();assert(saved.save(prefs,3) && !prefs.isKey(LevelReference::KEY));
    LevelReference absent;absent.load(prefs,3);assert(!absent.active);
    Preferences::data.clear();
  }
  std::cout<<"PASS: level reference, live tilt event bytes, 10 Hz gate, no event in a pull or before the fusion is ready; the page's one heading reference per opening keeps tilt, follows turns and holds the last pose\n";
}
