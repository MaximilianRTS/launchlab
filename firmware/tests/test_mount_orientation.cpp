#include "../LaunchLabMini/mount_orientation.h"
#include "../LaunchLabMini/orientation_estimator.h"
#include "../LaunchLabMini/motion_replay.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

using namespace LaunchMotion;
using MountOrientation::Mode;

static bool near(float a,float b,float tolerance=1e-6f){return std::fabs(a-b)<=tolerance;}
static float dot(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cross(const float *a,const float *b,float *out) {
  out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];
}
static void equalVector(const float *a,const float *b,float tolerance=1e-6f) {
  for(unsigned j=0;j<3;++j)assert(near(a[j],b[j],tolerance));
}
static void equalLevel(const StickS3Level::Reading &a,const StickS3Level::Reading &b) {
  assert(a.state==b.state);assert(near(a.degrees,b.degrees));
  assert(near(a.right,b.right));assert(near(a.down,b.down));
}
static void equalSample(const Sample &a,const Sample &b) {
  assert(a.us==b.us && a.clipped==b.clipped && a.fused==b.fused);
  assert(a.settled==b.settled && a.stationary==b.stationary);
  assert(std::memcmp(a.a,b.a,sizeof a.a)==0 && std::memcmp(a.g,b.g,sizeof a.g)==0);
  assert(std::memcmp(&a.q,&b.q,sizeof a.q)==0);
}

// Independent physical-basis construction, not an inverse call to the helper:
// Side +X = Top +Z, Side +Y = Top +Y, Side +Z = Top -X.
static void sidecarNative(const float *top,float *side) {
  side[0]=top[2];side[1]=top[1];side[2]=-top[0];
}
// Top turned 90 degrees, from the stated physical axes. L (counter-clockwise from
// above): Device +X = launcher +Y, device +Y = launcher -X. R is its mirror turn.
static void top90LNative(const float *top,float *device) {
  const float x=top[0],y=top[1],z=top[2];device[0]=y;device[1]=-x;device[2]=z;
}
static void top90RNative(const float *top,float *device) {
  const float x=top[0],y=top[1],z=top[2];device[0]=-y;device[1]=x;device[2]=z;
}
static float determinant(Mode mode) {
  const float basis[3][3]={{1,0,0},{0,1,0},{0,0,1}};float m[3][3];
  for(unsigned j=0;j<3;++j)MountOrientation::mapVector(mode,basis[j],m[j]);
  float c[3];cross(m[1],m[2],c);return dot(m[0],c);
}
static Sample measurement(uint64_t us,Quaternion pose,const float *rate,const float *bias,float acceleration=0) {
  Sample s;s.us=us;const float up[3]={acceleration,0,1};inverse(pose).rotate(up,s.a);
  for(unsigned j=0;j<3;++j)s.g[j]=rate[j]+bias[j];return s;
}

static void testMapping() {
  assert(MountOrientation::valid(0) && MountOrientation::valid(1));
  assert(MountOrientation::valid(2) && MountOrientation::valid(3) && MountOrientation::MODES==4);
  assert(!MountOrientation::valid(4) && !MountOrientation::valid(256));
  assert(std::strcmp(MountOrientation::label(Mode::Top),"Top")==0);
  assert(std::strcmp(MountOrientation::label(Mode::Sidecar),"Sidecar")==0);
  assert(std::strcmp(MountOrientation::label(Mode::Top90L),"Top 90 L")==0);
  assert(std::strcmp(MountOrientation::label(Mode::Top90R),"Top 90 R")==0);
  const float basis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
  const float expected[3][3]={{0,0,1},{0,1,0},{-1,0,0}};
  float mapped[3][3];
  for(unsigned j=0;j<3;++j) {
    MountOrientation::mapVector(Mode::Sidecar,basis[j],mapped[j]);equalVector(mapped[j],expected[j]);
  }
  float handed[3];cross(mapped[0],mapped[1],handed);equalVector(handed,mapped[2]);
  assert(near(dot(handed,mapped[2]),1)); // determinant +1, so gyro needs no reflection sign.
  const float a[3]={.3f,-.7f,1.1f},b[3]={-2.3f,4.7f,.8f};
  float ma[3],mb[3],ab[3],mab[3],mappedCross[3];
  MountOrientation::mapVector(Mode::Sidecar,a,ma);MountOrientation::mapVector(Mode::Sidecar,b,mb);
  assert(near(dot(a,a),dot(ma,ma)));assert(near(dot(a,b),dot(ma,mb)));
  cross(a,b,ab);MountOrientation::mapVector(Mode::Sidecar,ab,mab);cross(ma,mb,mappedCross);equalVector(mab,mappedCross);
  float inPlace[3]={a[0],a[1],a[2]};MountOrientation::mapVector(Mode::Sidecar,inPlace,inPlace);equalVector(inPlace,ma);
  // Top preserves the exact float representations, including signed zero and
  // a non-finite sample that the owning fusion path must reject itself.
  const float special[3]={-0.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()};
  float identity[3];MountOrientation::mapVector(Mode::Top,special,identity);
  assert(std::memcmp(special,identity,sizeof identity)==0);
  MountOrientation::mapVector(Mode::Top,identity,identity);assert(std::memcmp(special,identity,sizeof identity)==0);
  MountOrientation::mapVector(static_cast<Mode>(4),a,identity);assert(std::memcmp(a,identity,sizeof identity)==0);
  // An upright canonical launcher puts gravity in Sidecar native +X.
  const float sideUp[3]={1,0,0};float topUp[3];MountOrientation::mapVector(Mode::Sidecar,sideUp,topUp);
  const float expectedUp[3]={0,0,1};equalVector(topUp,expectedUp);
  const float tiltedTop[3]={std::sin(20/DEG),0,std::cos(20/DEG)};float tiltedSide[3],mappedTilt[3];
  sidecarNative(tiltedTop,tiltedSide);MountOrientation::mapVector(Mode::Sidecar,tiltedSide,mappedTilt);
  StickS3Level topLevel,sideLevel;topLevel.feed(tiltedTop[0],tiltedTop[1],tiltedTop[2],1);
  sideLevel.feed(mappedTilt[0],mappedTilt[1],mappedTilt[2],1);equalLevel(topLevel.reading(1),sideLevel.reading(1));
  assert(near(topLevel.reading(1).degrees,20,.0001f) && topLevel.reading(1).right>0);
}
static const Mode ALL[]={Mode::Top,Mode::Sidecar,Mode::Top90L,Mode::Top90R};
static void testRotatedMappings() {
  // The stated mappings, axis by axis. L: launcher = (-y, x, z). R: (y, -x, z).
  const float x[3]={1,0,0},y[3]={0,1,0},z[3]={0,0,1},minusX[3]={-1,0,0},minusY[3]={0,-1,0};
  float out[3];
  MountOrientation::mapVector(Mode::Top90L,x,out);equalVector(out,y);
  MountOrientation::mapVector(Mode::Top90L,y,out);equalVector(out,minusX);
  MountOrientation::mapVector(Mode::Top90L,z,out);equalVector(out,z);
  MountOrientation::mapVector(Mode::Top90R,x,out);equalVector(out,minusY);
  MountOrientation::mapVector(Mode::Top90R,y,out);equalVector(out,x);
  MountOrientation::mapVector(Mode::Top90R,z,out);equalVector(out,z);
  // Four quarter turns about +Z: L is the inverse of R, and L twice is a half turn.
  const float v[3]={.3f,-.7f,1.1f};float l[3],lr[3],ll[3];
  MountOrientation::mapVector(Mode::Top90L,v,l);MountOrientation::mapVector(Mode::Top90R,l,lr);equalVector(lr,v);
  MountOrientation::mapVector(Mode::Top90L,l,ll);const float half[3]={-v[0],-v[1],v[2]};equalVector(ll,half);
  // A turn measured against launcher axes: device +X along launcher +Y is 90
  // degrees counter-clockwise from launcher +X, seen from above (+Z toward you).
  assert(std::atan2(y[1],y[0])*DEG>0);
  const float b[3]={-2.3f,4.7f,.8f};
  for(const Mode mode:ALL) {
    // Proper rotations: determinant +1, lengths, angles and handedness kept, so
    // the same mapping is right for gyro and accelerometer.
    assert(near(determinant(mode),1));
    float ma[3],mb[3],ab[3],mab[3],mc[3];
    MountOrientation::mapVector(mode,v,ma);MountOrientation::mapVector(mode,b,mb);
    assert(near(dot(v,v),dot(ma,ma),1e-5f) && near(dot(v,b),dot(ma,mb),1e-5f));
    cross(v,b,ab);MountOrientation::mapVector(mode,ab,mab);cross(ma,mb,mc);equalVector(mab,mc,1e-5f);
    float inPlace[3]={v[0],v[1],v[2]};MountOrientation::mapVector(mode,inPlace,inPlace);equalVector(inPlace,ma);
    // unmapVector inverts mapVector, also in place.
    float back[3];MountOrientation::unmapVector(mode,ma,back);equalVector(back,v);
    MountOrientation::unmapVector(mode,ma,ma);equalVector(ma,v);
  }
  // Every pair: remapVector gives what the other Mounting reads of the sample.
  for(const Mode from:ALL)for(const Mode to:ALL) {
    float in[3],expect[3],got[3],there[3];
    MountOrientation::mapVector(from,v,in);MountOrientation::mapVector(to,v,expect);
    MountOrientation::remapVector(from,to,in,got);equalVector(got,expect);
    // Round trip, also aliased.
    MountOrientation::remapVector(to,from,got,there);equalVector(there,in);
    MountOrientation::remapVector(from,to,in,in);equalVector(in,expect);
  }
  // Gravity: a level launcher reads level in every Top turn, its screen up. The
  // ripcord down reads 90 degrees with the launcher's own +Y end down, whichever
  // stick edge that is. Tilt reads in launcher axes, not the stick's.
  for(const Mode mode:{Mode::Top,Mode::Top90L,Mode::Top90R}) {
    float mapped[3];MountOrientation::mapVector(mode,z,mapped);equalVector(mapped,z);
    StickS3Level level;level.feed(mapped[0],mapped[1],mapped[2],1);
    assert(level.reading(1).valid() && near(level.reading(1).degrees,0,.001f));
  }
  auto tiltedDevice=[](void (*native)(const float *,float *),float *device){
    // Launcher tipped 20 degrees: gravity-up gains launcher +X.
    const float top[3]={std::sin(20/DEG),0,std::cos(20/DEG)};native(top,device);};
  const float top[3]={std::sin(20/DEG),0,std::cos(20/DEG)};
  StickS3Level expected;expected.feed(top[0],top[1],top[2],1);
  struct Turned {Mode mode;void (*native)(const float *,float *);};
  for(const Turned t:{Turned{Mode::Top90L,top90LNative},Turned{Mode::Top90R,top90RNative}}) {
    float device[3],mapped[3];tiltedDevice(t.native,device);
    MountOrientation::mapVector(t.mode,device,mapped);equalVector(mapped,top);
    StickS3Level level;level.feed(mapped[0],mapped[1],mapped[2],1);equalLevel(level.reading(1),expected.reading(1));
    // Read as plain Top the same sample tips another way: the Mounting matters.
    StickS3Level plain;plain.feed(device[0],device[1],device[2],1);
    assert(near(plain.reading(1).degrees,20,.001f));
    assert(!near(plain.reading(1).right,expected.reading(1).right,.1f));
  }
  // Wear check's hint: level tells only Sidecar from the Top family, each way
  // it asks something a remap can actually show, and its captions name the mode.
  for(const Mode mode:ALL) {
    const Mode other=MountOrientation::levelAlternative(mode);
    assert((mode==Mode::Sidecar)==(other==Mode::Top));
    assert(other==Mode::Top || other==Mode::Sidecar);
    assert(std::strstr(MountOrientation::mountCaption(mode),MountOrientation::label(mode))==MountOrientation::mountCaption(mode));
    assert(std::strncmp(MountOrientation::hintCaption(mode),MountOrientation::label(other),std::strlen(MountOrientation::label(other)))==0);
    assert(std::strlen(MountOrientation::mountCaption(mode))*6<135 && std::strlen(MountOrientation::hintCaption(mode))*6<135);
  }
}

enum class Pipeline { DirectTop, MappedTop, MappedSidecar, MappedTop90L, MappedTop90R, UnmappedSidecar, AccelOnlySidecar };
struct Run {
  Trace trace;Sample last;std::vector<Sample> emitted;float bias[3]={};
  uint32_t updates=0,resets=0;float maximumTurnError=0;
};
static Run run(Pipeline pipeline) {
  LauncherOrientation filter;auto ring=std::make_unique<Ring>();Run out;
  const float zero[3]={},bias[3]={.8f,-.6f,.4f},tiltRate[3]={18,30,0};
  Quaternion tilted;tilted.integrate(tiltRate,1);
  auto feed=[&](const Sample &canonical) {
    Sample raw=canonical;
    if(pipeline==Pipeline::MappedSidecar || pipeline==Pipeline::UnmappedSidecar || pipeline==Pipeline::AccelOnlySidecar) {
      sidecarNative(canonical.a,raw.a);sidecarNative(canonical.g,raw.g);
    }
    if(pipeline==Pipeline::MappedTop90L){top90LNative(canonical.a,raw.a);top90LNative(canonical.g,raw.g);}
    if(pipeline==Pipeline::MappedTop90R){top90RNative(canonical.a,raw.a);top90RNative(canonical.g,raw.g);}
    if(pipeline==Pipeline::MappedTop || pipeline==Pipeline::MappedSidecar || pipeline==Pipeline::MappedTop90L || pipeline==Pipeline::MappedTop90R || pipeline==Pipeline::AccelOnlySidecar) {
      const Mode mode=pipeline==Pipeline::MappedTop?Mode::Top:pipeline==Pipeline::MappedTop90L?Mode::Top90L:pipeline==Pipeline::MappedTop90R?Mode::Top90R:Mode::Sidecar;
      MountOrientation::mapVector(mode,raw.a,raw.a);
      if(pipeline!=Pipeline::AccelOnlySidecar)MountOrientation::mapVector(mode,raw.g,raw.g);
    }
    filter.feed(raw,[&](const Sample &s){out.last=s;ring->feed(s);if(s.us>=30000000)out.emitted.push_back(s);});
  };
  for(unsigned i=0;i<=3000;++i)feed(measurement(1000000+i*10000,tilted,zero,bias));
  assert(out.last.settled && filter.rest());
  const Quaternion onset=out.last.q;Quaternion truth=tilted;
  const float rate[3]={120,-80,160};const uint32_t jitter[4]={7000,13000,11000,9000};uint64_t now=31000000;
  for(unsigned i=0;i<100;++i) {
    const auto dt=jitter[i%4];now+=dt;truth.integrate(rate,dt*1e-6f);
    feed(measurement(now,truth,rate,bias));
    Quaternion at=tilted;at.integrate(rate,(out.last.us-31000000)*1e-6f);
    out.maximumTurnError=std::max(out.maximumTurnError,Quaternion::difference(relative(onset,out.last.q),relative(tilted,at)));
  }
  out.trace=ring->build(31300000,31700000,6000,1);
  assert(out.trace.valid() && out.trace.fused && out.trace.startLevel.valid() && out.trace.endLevel.valid());
  filter.biasDegrees(out.bias);out.updates=filter.updates();out.resets=filter.resets();return out;
}

static void equalRun(const Run &a,const Run &b) {
  assert(a.updates==b.updates && a.resets==b.resets && a.emitted.size()==b.emitted.size());
  equalVector(a.bias,b.bias);equalSample(a.last,b.last);
  for(unsigned i=0;i<a.emitted.size();++i)equalSample(a.emitted[i],b.emitted[i]);
  equalLevel(a.trace.startLevel,b.trace.startLevel);equalLevel(a.trace.endLevel,b.trace.endLevel);
  Image ia,ib;encode(a.trace,7,ia);encode(b.trace,7,ib);assert(ia==ib);
  Trace decoded;uint32_t generation;assert(decode(ib,decoded,generation) && generation==7);
  Image roundtrip;encode(decoded,7,roundtrip);assert(roundtrip==ia);
  const auto comparison=compare(a.trace,decoded);
  // Compare's quaternion interpolation can round an identical packed pose;
  // byte equality above is the stronger canonical-storage invariant.
  assert(comparison.valid && comparison.turnDegrees<.05f && comparison.accelG<.00001f && comparison.gyroDps<.0001f);
  MotionReplay::Replay ra,rb;assert(ra.build(a.trace) && rb.build(decoded));
  for(unsigned i=0;i<MotionReplay::Replay::COUNT;++i) {
    assert(ra.ms[i]==rb.ms[i] && Quaternion::difference(ra.pose[i],rb.pose[i])<.001f);
    const auto va=MotionReplay::Launcher::axes(ra.pose[i],true),vb=MotionReplay::Launcher::axes(rb.pose[i],true);
    for(const auto &vertex:MotionReplay::Launcher::BODY)assert((va.at(vertex.low)-vb.at(vertex.low)).length()<1e-5f);
  }
  for(bool flip:{false,true}) {
    equalLevel(StickS3Level::forDisplay(a.trace.startLevel,flip),StickS3Level::forDisplay(b.trace.startLevel,flip));
    for(int ms=-400;ms<=550;ms+=17) {
      const auto qa=MotionReplay::forDisplay(ra.at(ms),flip),qb=MotionReplay::forDisplay(rb.at(ms),flip);
      assert(Quaternion::difference(qa,qb)<.001f);
      const auto pa=MotionReplay::camera(MotionReplay::Launcher::axes(qa,flip).at({2.1f,.3f,.08f}));
      const auto pb=MotionReplay::camera(MotionReplay::Launcher::axes(qb,flip).at({2.1f,.3f,.08f}));
      assert((pa-pb).length()<1e-5f);
    }
  }
}

static void testFrameReset() {
  LauncherOrientation reused,fresh;auto ring=std::make_unique<Ring>();Sample last;
  const float zero[3]={},bias[3]={.8f,-.6f,.4f};Quaternion tilted;const float turn[3]={0,30,0};tilted.integrate(turn,1);
  for(unsigned i=0;i<=3000;++i)reused.feed(measurement(1000000+i*10000,tilted,zero,bias),[&](const Sample &s){last=s;ring->feed(s);});
  assert(reused.rest() && last.settled && ring->size()>0);
  float learned[3];reused.biasDegrees(learned);for(unsigned j=0;j<3;++j)assert(near(learned[j],bias[j],.08f));
  const auto updatesBefore=reused.updates();reused.resetFrame();ring->clear();
  assert(reused.resets()==1 && reused.updates()==updatesBefore && ring->size()==0 && ring->latest()==0);
  reused.biasDegrees(learned);equalVector(learned,zero);assert(!reused.rest());
  // The new frame may start before the old resampling deadline. No old input,
  // bias or interpolation is used; its emitted samples equal a fresh filter.
  const float newBias[3]={-.4f,.2f,.7f};Quaternion pose;const float newTilt[3]={-20,10,0};pose.integrate(newTilt,1);
  std::vector<Sample> a,b;
  for(unsigned i=0;i<=150;++i) {
    const auto canonical=measurement(31005000+i*10000,pose,zero,newBias);Sample mapped=canonical;
    sidecarNative(canonical.a,mapped.a);sidecarNative(canonical.g,mapped.g);
    MountOrientation::mapVector(Mode::Sidecar,mapped.a,mapped.a);MountOrientation::mapVector(Mode::Sidecar,mapped.g,mapped.g);
    reused.feed(mapped,[&](const Sample &s){a.push_back(s);ring->feed(s);});fresh.feed(canonical,[&](const Sample &s){b.push_back(s);});
    assert(a.size()==i+1 && b.size()==i+1);equalSample(a.back(),b.back());
    assert(a.back().settled==(i>=150));
  }
  assert(a.front().us==31005000 && a[1].us==31015000 && a.back().settled);
  assert(ring->build(31005000,31405000,6000,2).quality==Quality::Warming);
}

int main() {
  testMapping();testRotatedMappings();
  const auto direct=run(Pipeline::DirectTop),top=run(Pipeline::MappedTop),side=run(Pipeline::MappedSidecar);
  equalRun(direct,top);equalRun(direct,side);assert(direct.maximumTurnError<1.5f);
  equalRun(direct,run(Pipeline::MappedTop90L));equalRun(direct,run(Pipeline::MappedTop90R));
  // These controls would fail if only acceleration were mapped, or if both
  // test paths merely shared a mock orientation transform.
  const auto unmapped=run(Pipeline::UnmappedSidecar),accelOnly=run(Pipeline::AccelOnlySidecar);
  const float up[3]={0,0,1};
  const auto unmappedStill=levelAt(unmapped.emitted.front().q,up),directStill=levelAt(direct.emitted.front().q,up);
  assert(unmappedStill.valid() && directStill.valid() && std::fabs(unmappedStill.degrees-directStill.degrees)>10);
  assert(Quaternion::difference(accelOnly.last.q,direct.last.q)>10);
  testFrameReset();
  std::cout<<"PASS: Top identity; Sidecar and Top 90 L/R proper physical rotations, every remapVector pair; alias-safe accel/gyro mapping; actual VQF, level, canonical capture/storage/replay equivalence; both Flip views; unmapped/gyro negative controls; clean frame reset and renewed warmup.\n";
}
