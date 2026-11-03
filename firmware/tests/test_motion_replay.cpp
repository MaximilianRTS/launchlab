#include "../LaunchLabMini/motion_replay.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
using namespace MotionReplay;
static bool near(float a,float b,float tolerance=.0001f){return std::fabs(a-b)<tolerance;}
static float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static bool nearVec(Vec a,Vec b,float tolerance=.0001f){return (a-b).length()<tolerance;}
static bool sameBox(const Launcher::Box &b,Vec low,Vec high){return nearVec(b.low,low) && nearVec(b.high,high);}
using MountOrientation::Mode;
static const Mode MOUNTS[]={Mode::Top,Mode::Sidecar,Mode::Top90L,Mode::Top90R};
// The spec's tables, written out independently of the model's own constants.
static void testLauncherGeometry() {
  using namespace Launcher;
  for(const Mode m:MOUNTS) {
    assert(sameBox(box(Head,m),{-.45f,-.45f,-.18f},{.45f,.45f,.15f}));
    assert(sameBox(box(Neck,m),{.45f,-.36f,-.14f},{.85f,.36f,.10f}));
    assert(sameBox(box(Barrel,m),{.85f,-.30f,-.16f},{2.10f,.30f,.08f}));
    assert(sameBox(box(Stem,m),{-.05f,.45f,-.05f},{.05f,.78f,.05f}));
    assert(sameBox(box(Bar,m),{-.22f,.78f,-.05f},{.22f,.86f,.05f}));
    assert(sameBox(box(Cord,m),{-.02f,-1.10f,-.02f},{.02f,-.45f,.02f}));
  }
  assert(sameBox(box(Stick,Mode::Top),{-.20f,-.38f,.15f},{.20f,.38f,.36f}));
  assert(sameBox(box(Screen,Mode::Top),{-.13f,-.10f,.36f},{.13f,.28f,.37f}));
  assert(sameBox(box(Stick,Mode::Top90R),{-.38f,.03f,.15f},{.38f,.43f,.36f}));
  assert(sameBox(box(Screen,Mode::Top90R),{-.10f,.10f,.36f},{.28f,.36f,.37f}));
  assert(sameBox(box(Stick,Mode::Top90L),{-.38f,-.43f,.15f},{.38f,-.03f,.36f}));
  assert(sameBox(box(Screen,Mode::Top90L),{-.28f,-.36f,.36f},{.10f,-.10f,.37f}));
  assert(sameBox(box(Stick,Mode::Sidecar),{-.66f,-.38f,-.20f},{-.45f,.38f,.20f}));
  assert(sameBox(box(Screen,Mode::Sidecar),{-.67f,-.10f,-.13f},{-.66f,.28f,.13f}));
  // Subtract the centre (0.6,-0.1,0), then scale by 0.62.
  assert(nearVec(place({.6f,-.1f,0}),{0,0,0}) && nearVec(place({1.6f,.9f,1}),{.62f,.62f,.62f}));
  // The screen sits nearer the stick's pin end: in Top that is +Y, and in the
  // turned mountings the end mapVector says the device's +Y lies along.
  for(const Mode m:MOUNTS) {
    const float pins[3]={0,1,0};float along[3];MountOrientation::mapVector(m,pins,along);
    const Box stick=box(Stick,m),screen=box(Screen,m);
    auto middle=[&](const Box &b){return dot((b.low+b.high)*.5f,{along[0],along[1],along[2]});};
    assert(middle(screen)>middle(stick));
    assert(screen.low.x>=stick.low.x-.011f && screen.high.x<=stick.high.x+.001f);
    assert(screen.low.y>=stick.low.y && screen.high.y<=stick.high.y);
  }
  // Top 90 R has its USB end (device -Y) pointing away from the barrel, L toward it.
  {
    const float usb[3]={0,-1,0};float r[3],l[3];
    MountOrientation::mapVector(Mode::Top90R,usb,r);MountOrientation::mapVector(Mode::Top90L,usb,l);
    assert(near(r[0],-1) && near(l[0],1));
  }
  // The corners and faces: each quad flat on its normal's outer side.
  const Box b=box(Head,Mode::Top);
  auto corner=[&](unsigned i){return Vec{i&1?b.high.x:b.low.x,i&2?b.high.y:b.low.y,i&4?b.high.z:b.low.z};};
  for(unsigned f=0;f<6;++f) {
    const float plane=dot(NORMALS[f],corner(QUADS[f][0]));assert(plane>0);
    for(unsigned i=0;i<4;++i)assert(near(dot(NORMALS[f],corner(QUADS[f][i])),plane));
    // Consecutive corners share an edge, so two triangles fill the quad.
    for(unsigned i=0;i<4;++i)assert(__builtin_popcount(QUADS[f][i]^QUADS[f][(i+1)%4])==1);
  }
  // The scene's scale fit: the projection is nearly orthonormal, so the model's
  // image is no longer than its reach from the rotation centre, which fits the frame.
  const Vec towardU{.70710678f,.70710678f,0},towardV{-.70710678f,.70710678f,0};
  assert(near(towardViewer(towardU),.663f,.001f) && near(towardViewer(towardV),.383f,.001f) && near(towardViewer({0,0,1}),.643f,.001f));
  // The view axis is what the projection flattens away.
  assert(camera(towardU*.663f+towardV*.383f+Vec{0,0,.643f}).length()<.002f);
  assert(nearVec(camera(towardU),{-.5f,.5568f,0},.001f) && nearVec(camera(towardV),{.866f,.3215f,0},.001f));
  assert(near(camera({0,0,1}).y,-.766f,.001f) && camera({0,0,1}).length()<=1);
  float reach=0;
  for(const Mode m:MOUNTS)for(unsigned p=0;p<PARTS;++p) {
    const Box part=box(Part(p),m);
    for(unsigned i=0;i<8;++i)reach=std::max(reach,place({i&1?part.high.x:part.low.x,i&2?part.high.y:part.low.y,i&4?part.high.z:part.low.z}).length());
  }
  assert(reach<1);
  Cube cube;float half=0,halfWide=0;
  for(unsigned i=0;i<8;++i){const Vec p=camera(cube.corner(i));half=std::max(half,std::fabs(p.y));halfWide=std::max(halfWide,std::fabs(p.x));}
  assert(reach<half && reach<halfWide);
  // The spec's floor: level, radius .95 at z -.32, closed, inside the scene.
  for(unsigned i=0;i<MotionReplay::Floor::SEGMENTS;++i) {
    const Vec p=MotionReplay::Floor::point(i),q=MotionReplay::Floor::point(i+1);
    assert(near(std::hypot(p.x,p.y),.95f) && near(p.z,-.32f) && near(viewU(p)*viewU(p)+viewV(p)*viewV(p),.95f*.95f,.001f));
    assert(near((q-p).length(),2*.95f*std::sin(3.14159265f/MotionReplay::Floor::SEGMENTS),.001f));
    assert(std::fabs(camera(p).x)<halfWide && std::fabs(camera(p).y)<half);
  }
  assert(nearVec(MotionReplay::Floor::point(MotionReplay::Floor::SEGMENTS),MotionReplay::Floor::point(0)));
  assert(near(MotionReplay::Floor::point(4).y,.95f) && near(MotionReplay::Floor::point(4).x,0));
}
// A small software canvas: fills a face with a depth test, or by overwriting
// as the painter does. Fine grid in scene units, not the device's pixels.
struct Raster {
  static constexpr int SIZE=240;static constexpr float SCALE=115.0f;
  std::vector<int> owner=std::vector<int>(SIZE*SIZE,-1);std::vector<float> depth=std::vector<float>(SIZE*SIZE,-1e9f);
  void fill(const Vec *world,const uint8_t *quad,int id,bool testDepth) {
    Vec image[4];float toward[4];
    for(unsigned i=0;i<4;++i){image[i]=camera(world[quad[i]]);toward[i]=Launcher::towardViewer(world[quad[i]]);}
    triangle(image[0],image[1],image[2],toward[0],toward[1],toward[2],id,testDepth);
    triangle(image[0],image[2],image[3],toward[0],toward[2],toward[3],id,testDepth);
  }
  void triangle(Vec a,Vec b,Vec c,float da,float db,float dc,int id,bool testDepth) {
    const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);if(std::fabs(area)<1e-7f)return;
    auto column=[](float v){return v*SCALE+SIZE/2;};
    const int x0=std::max(0,int(column(std::min({a.x,b.x,c.x})))),x1=std::min(SIZE-1,int(column(std::max({a.x,b.x,c.x})))+1);
    const int y0=std::max(0,int(column(std::min({a.y,b.y,c.y})))),y1=std::min(SIZE-1,int(column(std::max({a.y,b.y,c.y})))+1);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
      const float px=(x-SIZE/2)/SCALE,py=(y-SIZE/2)/SCALE;
      const float w0=((b.x-px)*(c.y-py)-(b.y-py)*(c.x-px))/area,w1=((c.x-px)*(a.y-py)-(c.y-py)*(a.x-px))/area,w2=1-w0-w1;
      if(w0<0 || w1<0 || w2<0)continue;
      const float d=w0*da+w1*db+w2*dc;const int at=y*SIZE+x;
      if(testDepth && d<depth[at])continue;
      depth[at]=d;owner[at]=id;
    }
  }
};
// The painter against a depth buffer, every Mounting and both Flips. The screen
// is left out of both: it is drawn over the stick whatever the depth.
static void testPainterMatchesDepth() {
  using namespace Launcher;
  float worst=0;unsigned poses=0,mostParts=0,mostTriangles=0;
  for(int yaw=0;yaw<360;yaw+=30)for(int pitch=-90;pitch<=90;pitch+=30)for(int roll=-180;roll<180;roll+=30) {
    LaunchMotion::Quaternion q;const float r[3]={float(roll),0,0},p[3]={0,float(pitch),0},h[3]={0,0,float(yaw)};
    LaunchMotion::Quaternion a,b,c;a.integrate(r,1);b.integrate(p,1);c.integrate(h,1);
    q=LaunchMotion::multiply(c,LaunchMotion::multiply(b,a));
    for(const bool flip:{false,true})for(const Mode mount:MOUNTS) {
      const auto view=Launcher::axes(forDisplay(q,flip),flip,mount);
      Raster painter,truth;unsigned triangles=0;int lastPart=-1,stickAt=-1,sequence=0,partsSeen=0;
      paint(view,mount,[&](Part part,const Vec *world,const uint8_t *quad,Vec normal) {
        assert(towardViewer(normal)>0);triangles+=2;++sequence;
        if(int(part)!=lastPart){lastPart=part;++partsSeen;}
        if(part==Stick)stickAt=sequence;
        if(part==Screen){assert(stickAt>0);return;}
        const int id=int(part)*6+int(quad-QUADS[0])/4;
        painter.fill(world,quad,id,false);truth.fill(world,quad,id,true);
      });
      mostParts=std::max<unsigned>(mostParts,partsSeen);
      // truth above ran depth-tested in the same calls, so it is order-free.
      unsigned covered=0,wrong=0;
      for(int i=0;i<Raster::SIZE*Raster::SIZE;++i) {
        if(truth.owner[i]<0){assert(painter.owner[i]<0);continue;}
        ++covered;
        // Faces meeting at a shared plane tie in depth: either owner is right.
        if(painter.owner[i]!=truth.owner[i] && truth.depth[i]-painter.depth[i]>.01f)++wrong;
      }
      assert(covered>0);worst=std::max(worst,float(wrong)/covered);++poses;
      mostTriangles=std::max(mostTriangles,triangles);
    }
  }
  assert(poses==12*7*12*2*4);
  std::cout<<"painter vs depth test: worst wrong share "<<worst*100<<"% over "<<poses<<" poses; at most "<<mostTriangles<<" triangles\n";
  assert(worst<.005f);assert(mostParts==PARTS);assert(mostTriangles<=48);
}
// The view, Flip and tilt on the actual drawn points.
static void testLauncherPose() {
  using namespace Launcher;
  LaunchMotion::Quaternion tilted;const float rate[3]={37,-58,23};tilted.integrate(rate,1);
  const LaunchMotion::Quaternion neutral{std::cos(.785398163f*.5f),0,0,std::sin(.785398163f*.5f)};
  const auto q=LaunchMotion::multiply(neutral,tilted);
  // True to the glass, on the screen's own x right, y down, per Mounting and Flip.
  // Flat, a horizontal direction along the screen's vertical is foreshortened by
  // the view's elevation: .643; across it, not at all.
  struct Look {Mode mount;float barrelX,barrelY,handleX,handleY;};
  const float flat=.643f;
  for(const Look look:{Look{Mode::Top90R,0,flat,1,0},Look{Mode::Top90L,0,-flat,-1,0},Look{Mode::Top,-1,0,0,flat},Look{Mode::Sidecar,-1,0,0,flat}})for(const bool flip:{false,true}) {
    const auto view=axes(forDisplay(neutral,flip),flip,look.mount);
    const float sign=flip?-1:1;
    assert(nearVec(camera(view.at({1,0,0})),{sign*look.barrelX,sign*look.barrelY,0},.002f));
    assert(nearVec(camera(view.at({0,1,0})),{sign*look.handleX,sign*look.handleY,0},.002f));
    // Independent of that table, from the device axes and the IMU's own mapping:
    // +Y pin end, +X = Y x Z; Flip Off (rotation 2) has screen top -Y, right -X.
    float top[3],right[3];
    const float screenTop[3]={0,flip?1.0f:-1.0f,0},screenRight[3]={flip?1.0f:-1.0f,0,0};
    MountOrientation::mapVector(look.mount,screenTop,top);MountOrientation::mapVector(look.mount,screenRight,right);
    assert(nearVec(camera(view.at({top[0],top[1],top[2]})),{0,-flat,0},.002f));
    // A Sidecar's glass stands upright, its right along gravity: the view keeps up
    // as up, so only its top can be true.
    if(look.mount!=Mode::Sidecar)assert(nearVec(camera(view.at({right[0],right[1],right[2]})),{1,0,0},.002f));
    // Up stays up, seen from above.
    assert(towardViewer(view.z)>0 && near(camera(view.z).y,-.766f,.001f) && near(camera(view.z).x,0,.001f));
  }
  // The photos' cases in words. Top 90 R, Flip Off: screen top is the USB end, away
  // from the barrel. Top: the barrel is at the screen's edge nearest it.
  {
    const float usb[3]={0,-1,0};float l[3];MountOrientation::mapVector(Mode::Top90R,usb,l);assert(near(l[0],-1));
    const auto r=axes(forDisplay(neutral,false),false,Mode::Top90R);
    assert(camera(r.at({1,0,0})).y>.6f && camera(r.at({0,1,0})).x>.9f);
    const float side[3]={-1,0,0};MountOrientation::mapVector(Mode::Top,side,l);assert(near(l[0],-1)); // Flip Off's right edge is away from the barrel.
    assert(camera(axes(forDisplay(neutral,false),false,Mode::Top).at({1,0,0})).x<-.9f);
  }
  // Every Mounting and Flip only turns about the vertical: height, so tilt, is kept,
  // and lengths: the app's View of the same pose differs by the turn alone.
  for(const Vec v:{Vec{2.1f,.3f,.08f},Vec{-.22f,.86f,.05f},Vec{.02f,-1.1f,.02f},Vec{-.38f,.43f,.36f}}) {
    const Vec posed=rotate(q,v);
    for(const Mode m:MOUNTS)for(const bool flip:{false,true}) {
      const Vec drawn=axes(forDisplay(q,flip),flip,m).at(v);
      assert(near(drawn.z,posed.z,.001f) && near(drawn.length(),v.length(),.001f));
    }
  }
  const auto mine=axes(forDisplay(q,false),false,Mode::Top90R);
  // The end that physically rises is the end the model raises.
  LaunchMotion::Quaternion rise;const float raise[3]={20,0,0};rise.integrate(raise,1); // +Y end up
  for(const Mode m:MOUNTS)for(const bool flip:{false,true}) {
    const auto view=axes(forDisplay(LaunchMotion::multiply(neutral,rise),flip),flip,m);
    assert(view.at({0,.86f,0}).z>.2f && view.at({0,-1.1f,0}).z<-.2f);
  }
  // A level launcher lights its top more than its sides, and light stays in range.
  const auto level=axes(forDisplay(neutral,false),false,Mode::Top90R);
  assert(light(level.z)>light(level.x) && light(level.z)>light(level.y));
  for(const Vec n:NORMALS){const float lit=light(mine.at(n));assert(lit>=0 && lit<=1.01f);}
}
static LaunchMotion::Trace trace(float acceleration=0,float heading=0);
// Get replay's payload: exactly what the recap draws, before forDisplay.
static void testPhonePayload() {
  auto replay=std::make_unique<Replay>();auto shown=std::make_unique<Replay>();
  std::vector<uint8_t> out(PHONE_BYTES+8,0xa5);
  auto i16=[&](size_t at){return int16_t(uint16_t(out[at])|uint16_t(out[at+1])<<8);};
  auto u32=[&](size_t at){return uint32_t(out[at])|uint32_t(out[at+1])<<8|uint32_t(out[at+2])<<16|uint32_t(out[at+3])<<24;};
  auto t=trace();t.number=77;
  assert(PHONE_BYTES==11+48*12 && PHONE_BYTES<=4096);
  assert(encodeForPhone(t,*replay,out.data(),out.size())==PHONE_BYTES);
  assert(out[PHONE_BYTES]==0xa5);
  assert(u32(0)==77 && u32(4)==400 && i16(8)==48 && out[10]==0);
  assert(shown->build(t));
  for(unsigned i=0;i<Replay::COUNT;++i) {
    const size_t at=11+i*12;
    assert(int32_t(u32(at))==shown->ms[i]);
    const LaunchMotion::Quaternion q{i16(at+4)/16384.0f,i16(at+6)/16384.0f,i16(at+8)/16384.0f,i16(at+10)/16384.0f};
    assert(LaunchMotion::Quaternion::difference(q,shown->pose[i])<.02f);
    assert(near(std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z),1,.001f));
  }
  // Time zero lies inside the replay, which starts before it and ends after it,
  // and durationMs is on the same clock.
  assert(shown->ms[0]<0 && shown->ms[47]>int(t.durationMs));
  // Heading is the display's: the barrel starts at 45 degrees, whatever the compass said.
  const Vec width=rotate(shown->at(0),{1,0,0});assert(near(width.x,width.y,.002f) && width.x>0);
  auto header=[&](const LaunchMotion::Trace &bad){
    return encodeForPhone(bad,*replay,out.data(),out.size())==PHONE_HEADER_BYTES && i16(8)==0 && out[10]==0;};
  auto bad=trace();bad.number=78;bad.quality=LaunchMotion::Quality::Gap;assert(header(bad) && u32(0)==78);
  bad=trace();bad.count=11;assert(header(bad));
  bad=trace();bad.fused=false;assert(header(bad));
  assert(header(LaunchMotion::Trace{}) && u32(0)==0);
  // What the device marks Tilt approx. it still draws: all 48 points, flagged,
  // with the same poses as an unmarked trace.
  auto saturated=trace();saturated.saturated=true;auto bridged=trace();bridged.bridged=true;
  for(const auto &marked:{saturated,bridged}) {
    assert(encodeForPhone(marked,*replay,out.data(),out.size())==PHONE_BYTES);
    assert(i16(8)==48 && out[10]==1 && replay->valid);
    for(unsigned i=0;i<Replay::COUNT;++i)assert(int32_t(u32(11+i*12))==shown->ms[i]);
  }
  // An unusable trace never carries the mark.
  auto markedBad=trace();markedBad.saturated=true;markedBad.fused=false;assert(header(markedBad));
  assert(encodeForPhone(t,*replay,out.data(),PHONE_BYTES-1)==0);
}
static LaunchMotion::Trace trace(float acceleration,float heading) {
  LaunchMotion::Trace t;t.quality=LaunchMotion::Quality::Valid;t.fused=true;t.rpm=6000;t.durationMs=400;t.count=48;t.gravity[2]=1;
  const float yaw[3]={0,0,heading},turn[3]={0,90,0};LaunchMotion::Quaternion world;world.integrate(yaw,1);
  for(unsigned i=0;i<t.count;++i) {
    auto &p=t.points[i];p.ms=-500+int(i*1150/47);p.a[0]=std::lround(acceleration*1000);
    LaunchMotion::Quaternion body;body.integrate(turn,(p.ms+500)*.001f);
    const auto q=LaunchMotion::multiply(world,body);
    p.q[0]=std::lround(q.w*16384);p.q[1]=std::lround(q.x*16384);p.q[2]=std::lround(q.y*16384);p.q[3]=std::lround(q.z*16384);
  }
  return t;
}
int main() {
  testLauncherGeometry();testPainterMatchesDepth();testLauncherPose();testPhonePayload();
  // The spec's View, by hand: (u,v)=((x+y)*.7071,(y-x)*.7071),
  // sx=v*.866-u*.5, sy=(u*.866+v*.5)*.643-z*.766.
  {
    const float u=(.3f+.5f)*.7071f,w=(.5f-.3f)*.7071f;
    assert(nearVec(camera({.3f,.5f,.2f}),{w*.866f-u*.5f,(u*.866f+w*.5f)*.643f-.2f*.766f,0},.001f));
  }
  assert(camera({0,0,1}).y<0 && near(camera({0,0,1}).x,0));
  {
    // A heading kept from Live tilt takes the place of the one from the pull's start.
    Replay own,kept;const float half=.5f*1.5707963f;
    const LaunchMotion::Quaternion turn{std::cos(half),0,0,std::sin(half)};
    assert(own.build(trace()) && kept.build(trace(),&turn));
    const auto from=rotate(own.pose[0],{1,0,0}),to=rotate(kept.pose[0],{1,0,0});
    assert(std::fabs(std::hypot(from.x,from.y)-std::hypot(to.x,to.y))<.001f);
    assert(std::fabs(from.z-to.z)<.001f);
    assert(std::fabs(from.x-to.x)+std::fabs(from.y-to.y)>.01f);
    // They differ only by a turn about vertical: roll and pitch stay real.
    const auto up=rotate(LaunchMotion::multiply(kept.pose[0],LaunchMotion::inverse(own.pose[0])),{0,0,1});
    assert(std::fabs(up.z-1)<.001f);
  }
  {
    // The ripcord goes out with the turns, on the replay's clock, and leaves at the fastest.
    struct {unsigned count=4;uint16_t ms[4]={100,200,300,400},rpm[4]={1000,3000,5000,4000},leadMs=100;} c;
    assert(ripcordAt(c,-50)==0.0f);
    assert(std::fabs(ripcordAt(c,50)-.25f)<.001f);
    assert(std::fabs(ripcordAt(c,100)-.5f)<.001f);
    assert(ripcordAt(c,200)==1.0f && ripcordAt(c,900)==1.0f);
    c.count=1;assert(ripcordAt(c,900)==0.0f);
    c.count=4;c.rpm[0]=9000;assert(ripcordAt(c,900)==0.0f);
    // Out, the handle stays whole; gone, no face of it or its cord is drawn.
    using namespace Launcher;
    Axes flat{{1,0,0},{0,1,0},{0,0,1}};unsigned parts=0;bool cord=false;
    paint(flat,MountOrientation::Mode::Top,[&](Part p,const Vec *,const uint8_t *,Vec){++parts;cord|=p==Bar || p==Stem || p==Cord;},.5f);
    assert(parts>0 && cord);
    cord=false;
    paint(flat,MountOrientation::Mode::Top,[&](Part p,const Vec *,const uint8_t *,Vec){cord|=p==Bar || p==Stem || p==Cord;},1.0f);
    assert(!cord);
  }
  Replay rotation,translated,heading;assert(rotation.build(trace()) && translated.build(trace(2)) && heading.build(trace(0,123)));
  assert(rotation.ms[0]==-500 && rotation.ms[47]==650);
  for(unsigned i=0;i<Replay::COUNT;++i) {
    assert(LaunchMotion::Quaternion::difference(rotation.pose[i],translated.pose[i])<.05f);
    assert(LaunchMotion::Quaternion::difference(rotation.pose[i],heading.pose[i])<.05f);
    // Rotation only: every model point keeps its reach from the fixed centre.
    Cube cube;const auto view=Launcher::axes(rotation.pose[i],true);
    for(const auto &part:Launcher::BODY)assert(near(view.at(part.low).length(),part.low.length(),.001f));
    assert(near(cube.high.x-cube.low.x,cube.high.y-cube.low.y) && near(cube.high.x-cube.low.x,cube.high.z-cube.low.z));
  }
  // Smooth optical-onset/end lookup clamps safely and preserves measured pose.
  assert(LaunchMotion::Quaternion::difference(rotation.at(-1000),rotation.pose[0])<.01f);
  assert(LaunchMotion::Quaternion::difference(rotation.at(2000),rotation.pose[47])<.01f);
  for(unsigned i=1;i<Replay::COUNT;++i) {
    assert(LaunchMotion::Quaternion::difference(rotation.at(rotation.ms[i]),rotation.pose[i])<.01f);
    const int mid=(rotation.ms[i-1]+rotation.ms[i])/2;
    assert(LaunchMotion::Quaternion::difference(rotation.at(mid),rotation.pose[i-1])<2);
  }
  // Initial tilt is retained, not silently flattened at optical onset.
  const LaunchMotion::Quaternion neutralHeading{std::cos(.785398163f*.5f),0,0,std::sin(.785398163f*.5f)};
  assert(near(LaunchMotion::Quaternion::difference(rotation.pose[0],neutralHeading),0,.03f));
  assert(near(LaunchMotion::Quaternion::difference(rotation.at(0),neutralHeading),45,.05f));
  assert(near(LaunchMotion::Quaternion::difference(rotation.at(400),neutralHeading),81,.05f));
  assert(near(LaunchMotion::Quaternion::difference(rotation.pose[47],neutralHeading),103.5f,.03f));
  const float up[3]={0,0,1};float actual[3],shown[3];
  for(unsigned i=0;i<Replay::COUNT;++i) {
    LaunchMotion::Interpolated original;assert(LaunchMotion::interpolate(trace(),rotation.ms[i],original));
    LaunchMotion::inverse(original.q).rotate(up,actual);LaunchMotion::inverse(rotation.pose[i]).rotate(up,shown);
    for(unsigned j=0;j<3;++j)assert(near(actual[j],shown[j],.0002f));
  }
  // An upright launcher starts its +X at 45 degrees in the horizontal plane, with
  // its +Y gravity-up. This is a display-heading choice only; no position or
  // measured inclination changes.
  auto upright=trace();LaunchMotion::Quaternion uprightQ;const float tip[3]={90,0,0};uprightQ.integrate(tip,1);
  for(unsigned i=0;i<upright.count;++i) {
    auto &p=upright.points[i];p.q[0]=std::lround(uprightQ.w*16384);p.q[1]=std::lround(uprightQ.x*16384);p.q[2]=0;p.q[3]=0;
  }
  Replay facing;assert(facing.build(upright));
  const Vec width=rotate(facing.pose[0],{1,0,0}),top=rotate(facing.pose[0],{0,1,0}),front=rotate(facing.pose[0],{0,0,1});
  assert(near(width.x,.70710678f) && near(width.y,.70710678f) && near(viewU(width),1) && near(viewV(width),0));
  assert(near(top.x,0) && near(top.y,0) && near(top.z,1));
  assert(front.x>0 && front.y<0 && near(front.x,-front.y));
  // The same screen-relative motion with USB down and USB up must produce the
  // same displayed pose. Simulate physical mounting inversion on the samples,
  // independently of the display helper, before resetting compass heading.
  auto mixed=trace(0,123),upsideDown=mixed;
  for(unsigned i=0;i<mixed.count;++i) {
    auto &p=mixed.points[i];LaunchMotion::Quaternion q;
    const float rate[3]={37,-58,23};q.integrate(rate,(p.ms+500)*.001f);
    const float yaw[3]={0,0,123};LaunchMotion::Quaternion h;h.integrate(yaw,1);
    q=LaunchMotion::multiply(h,q);
    p.q[0]=std::lround(q.w*16384);p.q[1]=std::lround(q.x*16384);p.q[2]=std::lround(q.y*16384);p.q[3]=std::lround(q.z*16384);
    auto &u=upsideDown.points[i];u=p;
    u.q[0]=-p.q[3];u.q[1]=p.q[2];u.q[2]=-p.q[1];u.q[3]=p.q[0];
  }
  Replay nativeMount,invertedMount;assert(nativeMount.build(mixed) && invertedMount.build(upsideDown));
  for(unsigned i=0;i<Replay::COUNT;++i) {
    const auto native=nativeMount.pose[i],displayed=forDisplay(invertedMount.pose[i],false);
    assert(LaunchMotion::Quaternion::difference(native,displayed)<.05f);
    assert(LaunchMotion::Quaternion::difference(native,forDisplay(native,true))<.05f);
    assert(LaunchMotion::Quaternion::difference(native,forDisplay(forDisplay(native,false),false))<.05f);
    const auto before=rotate(LaunchMotion::inverse(native),{0,0,1});
    const auto after=rotate(LaunchMotion::inverse(forDisplay(native,false)),{0,0,1});
    assert(near(after.x,-before.x) && near(after.y,-before.y) && near(after.z,before.z));
  }
  assert(LaunchMotion::Quaternion::difference(neutralHeading,forDisplay(neutralHeading,false))<.05f);
  auto missing=trace();missing.quality=LaunchMotion::Quality::Gap;assert(!rotation.build(missing) && !rotation.valid);
  missing=trace();missing.fused=false;assert(!rotation.build(missing));
  missing=trace();missing.points[0].ms=10;assert(!rotation.build(missing));
  std::cout<<"PASS: fixed-center rotation only, barrel starts at 45 degrees in the horizontal plane, both display orientations agree for physical mounting inversion, level-axis agreement, measured tilt preserved, acceleration cannot create travel, arbitrary heading canceled, constant scene scale, launcher model per the spec, painter order against a depth test, both Flip views, the BLE replay payload, pre/post capture, and legacy/invalid rejection\n";
}
