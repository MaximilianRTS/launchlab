#pragma once
#include "analog_tachometer.h"
#include "launch_motion.h"
#include "level_indicator.h"
#include "motion_replay.h"
#include "palette.h"

namespace MotionUI {
inline const char *unavailableTilt(LaunchMotion::Quality quality) {
  switch(quality) {
    case LaunchMotion::Quality::Warming:return "Tilt warming up";
    case LaunchMotion::Quality::Gap:return "Tilt incomplete";
    case LaunchMotion::Quality::Clipped:return "Tilt out of range";
    case LaunchMotion::Quality::TooLong:return "Pull too long";
    default:return "No tilt data";
  }
}
// The launcher at one pose over the stationary floor: the recap's and Live tilt's.
inline void launcherScene(lgfx::LGFXBase &g,const LaunchMotion::Quaternion &pose,int top,int height,
                          const Palette::Colors &c,bool flipEnabled,MountOrientation::Mode mount,float ripcord=0) {
  using namespace MotionReplay;
  Cube cube;Vec lo{1e9f,1e9f,0},hi{-1e9f,-1e9f,0};
  for(unsigned i=0;i<8;++i){const Vec p=camera(cube.corner(i));lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);}
  const float scale=std::min((g.width()-12)/(hi.x-lo.x),(height-4)/(hi.y-lo.y));
  auto project=[&](Vec v){const Vec p=camera(v);return Vec{g.width()*.5f+(p.x-(lo.x+hi.x)*.5f)*scale,top+height*.5f+(p.y-(lo.y+hi.y)*.5f)*scale,0};};
  auto line=[&](Vec a,Vec b,uint16_t color){a=project(a);b=project(b);g.drawLine(lroundf(a.x),lroundf(a.y),lroundf(b.x),lroundf(b.y),color);};
  // The stationary floor, the gravity reference the launcher tilts against: its
  // nearer half a little brighter.
  for(unsigned i=0;i<Floor::SEGMENTS;++i) {
    const Vec from=Floor::point(i),to=Floor::point(i+1);
    line(from,to,Launcher::towardViewer({from.x+to.x,from.y+to.y,0})>0?c.scenePlane:c.sceneRear);
  }
  const auto axes=Launcher::axes(forDisplay(pose,flipEnabled),flipEnabled,mount);
  // Flat shading in theme colors: a dark head, the launcher in the accent, a
  // light stick and its near-black screen, unshaded so it always reads.
  Launcher::paint(axes,mount,[&](Launcher::Part part,const Vec *world,const uint8_t *quad,Vec normal) {
    const uint16_t base=part==Launcher::Head?Palette::mix(c.background,c.muted,2,3):part==Launcher::Stick?c.text:c.accent;
    const uint16_t color=part==Launcher::Screen?c.sceneRear:Palette::mix(c.background,base,int(6+Launcher::light(normal)*10),16);
    int x[4],y[4];
    for(unsigned i=0;i<4;++i){const Vec p=project(world[quad[i]]);x[i]=lroundf(p.x);y[i]=lroundf(p.y);}
    g.fillTriangle(x[0],y[0],x[1],y[1],x[2],y[2],color);
    g.fillTriangle(x[0],y[0],x[2],y[2],x[3],y[3],color);
  },ripcord);
}

inline void spatial(lgfx::LGFXBase &g,const LaunchMotion::Trace &trial,
                    int top,int height,const Palette::Colors &c,float progress=1,bool flipEnabled=false,
                    MountOrientation::Mode mount=MountOrientation::Mode::Top,
                    const AnalogTachometer::Curve *rpmCurve=nullptr) {
  using namespace MotionReplay;
  // Fixed camera/scale and fused orientation only. Cached geometry lives off
  // the UI task stack; no live IMU, acquisition, travel or height inference.
  static Replay current;static uint32_t lastCurrent=0;
  auto signature=[](const LaunchMotion::Trace &t){
    return LaunchMotion::checksum(reinterpret_cast<const uint8_t *>(t.points),sizeof(t.points))^
      LaunchMotion::checksum(reinterpret_cast<const uint8_t *>(t.gravity),sizeof(t.gravity))^
      t.durationMs^(t.count<<16)^(unsigned(t.quality)<<24)^(unsigned(t.fused)<<30)^(levelReference.generation*2654435761u)^
      ((keptHeading.set?keptHeading.generation+1:0)*40503u);
  };
  const auto a=signature(trial);
  if(a!=lastCurrent){current.build(trial,keptHeading.set?&keptHeading.q:nullptr);lastCurrent=a;}
  if(!current.valid)return;
  progress=std::max(0.0f,std::min(1.0f,progress));
  // Full recorded context at original speed. Start/End circles retain optical
  // onset/end tilt, independent of the pre/post context shown in the replay.
  launcherScene(g,current.at(sourceTime(trial,progress)),top,height,c,flipEnabled,mount,
    rpmCurve?ripcordAt(*rpmCurve,sourceTime(trial,progress)):0);
}
// The Live tilt page: the fused pose on the recap's model. `pose` is the last one,
// and `live` says whether it is current: the fusion stops only in a flash write
// or before it has settled, so the page says so rather than guess.
inline void liveTilt(lgfx::LGFXBase &g,const LaunchMotion::Quaternion *pose,bool live,float degrees,
                     const Palette::Colors &c,bool flipEnabled,MountOrientation::Mode mount) {
  const int cx=g.width()/2;
  if(pose)launcherScene(g,*pose,49,112,c,flipEnabled,mount);
  char text[24];
  if(!pose)snprintf(text,sizeof(text),"Waiting for tilt");
  else if(live)snprintf(text,sizeof(text),"%.1f deg",degrees);
  else snprintf(text,sizeof(text),"Last reading");
  g.setTextColor(live?c.text:c.muted,c.background);
  g.drawString(text,cx,170,2);
}

// RPM in thousands for the curve's scale: "4K" for a whole thousand,
// "6.5K" below 10,000, "16K" above.
inline void thousands(char *text,size_t size,float rpm) {
  const long hundreds=lroundf(rpm/100);
  if(hundreds%10==0 || rpm>=9950)snprintf(text,size,"%ldK",(hundreds+5)/10);else snprintf(text,size,"%.1fK",hundreds/10.0f);
}
// The scale's reference lines below the peak: the smallest round step at
// least `minimum` RPM, so lines and their labels keep apart.
inline float curveStep(float minimum) {
  static constexpr uint16_t hundreds[]={5,10,15,20,25,30,40,50,60,80,100,150,200,250,300,400,500};
  for(const uint16_t h:hundreds)if(h*100.0f>=minimum)return h*100.0f;
  return 50000;
}
// The pull's RPM turn by turn (0.20.5), in the box left..right x top..bottom:
// time across, 0 RPM at the foot and the fastest turn at the top line,
// labelled in thousands, with faint round reference lines below it. Each
// turn is a dot on the accent line, the fastest larger.
inline void curve(lgfx::LGFXBase &g,const AnalogTachometer::Curve &curve,
                  int left,int top,int right,int bottom,const Palette::Colors &c) {
  g.drawFastHLine(left,bottom,right-left+1,c.faint);
  if(curve.count<2) {
    g.setTextColor(c.muted,c.background);g.drawString("No RPM curve",g.width()/2,(top+bottom)/2-4,1);return;
  }
  const float peak=curve.peak();
  const unsigned span=std::max<unsigned>(1,curve.ms[curve.count-1]);
  auto x=[&](unsigned i){return left+int(lroundf(float(curve.ms[i])*(right-left)/span));};
  auto y=[&](float value){return bottom-int(lroundf(std::min(1.0f,value/peak)*(bottom-top)));};
  char text[12];g.setTextDatum(top_right);g.setTextColor(c.muted,c.background);
  auto scaleLine=[&](float value){
    const int ly=y(value);for(int dx=left;dx<=right;dx+=4)g.drawFastHLine(dx,ly,2,c.faint);
    thousands(text,sizeof(text),value);g.drawString(text,left-3,ly-3,1);
  };
  scaleLine(peak);
  // Lines at least GAP px apart, and as far below the peak's: a label is 8 px.
  static constexpr int GAP=10;const float minimum=peak*GAP/(bottom-top);
  const float step=curveStep(minimum);
  for(unsigned k=1;k<=3 && peak-k*step>=minimum;++k)scaleLine(k*step);
  g.setTextDatum(top_center);
  // The rise starts from rest at the left edge.
  int px=left,py=bottom;unsigned fastest=0;
  for(unsigned i=0;i<curve.count;++i) {
    const int qx=x(i),qy=y(curve.rpm[i]);
    g.drawLine(px,py,qx,qy,c.accent);px=qx;py=qy;
    if(curve.rpm[i]>curve.rpm[fastest])fastest=i;
  }
  for(unsigned i=0;i<curve.count;++i)if(i!=fastest)g.fillCircle(x(i),y(curve.rpm[i]),1,c.accent);
  g.fillCircle(x(fastest),y(curve.rpm[fastest]),2,c.accent);
}
// A smooth demo pull peaking at `rpm`: the demo recap's curve.
inline const AnalogTachometer::Curve &demoCurve(float rpm=6500) {
  static AnalogTachometer::Curve c;static float made=-1;
  if(made!=rpm) {
    c.clear();static constexpr float shape[]={.22f,.4f,.58f,.73f,.85f,.94f,1,.97f,.88f,.76f,.62f,.5f};
    uint32_t endUs=0;
    for(const float f:shape){const uint32_t period=uint32_t(60000000.0f/(rpm*f));endUs+=period;c.add(endUs/1000,period);}
    made=rpm;
  }
  return c;
}
inline void feedback(lgfx::LGFXBase &g,const LaunchMotion::Trace &trial,
                     bool capturing,bool demo=false,float progress=1,bool flipEnabled=false,
                     const Palette::Colors &c=Palette::builtIn(0x07ff,true),const char *badge=nullptr,
                     const AnalogTachometer::Curve &rpmCurve=AnalogTachometer::Curve(),
                     MountOrientation::Mode mount=MountOrientation::Mode::Top) {
  const int cx=g.width()/2;char text[64];
  g.setTextDatum(top_center);
  // A celebrated pull's label: NEW BEST, or the milestone it reached.
  g.setTextColor(demo?c.warning:badge?c.accent:c.muted,c.background);g.drawString(demo?"DEMO":badge?badge:"Last launch",cx,2,1);
  snprintf(text,sizeof(text),"%.0f RPM",trial.rpm);g.setTextColor(c.accent,c.background);
  g.drawString(text,cx,17,g.textWidth(text,&fonts::Font4)<=g.width()-6?4:2);
  if(trial.valid() && trial.fused && !capturing) {
    spatial(g,trial,49,112,c,progress,flipEnabled,mount,&rpmCurve);
    // A full-scale reading or a bridged short gap is approximate: still show it, marked.
    g.setTextColor(trial.approximate()?c.warning:c.muted,c.background);
    g.drawString(trial.approximate()?"Tilt approx.":"Tilt replay",cx,164,1);
    // The replay's progress runs down the right edge, clear of the RPM curve.
    const int track=g.width()-4;
    g.drawFastVLine(track,53,105,c.faint);
    g.fillCircle(track,53+lroundf(std::max(0.0f,std::min(1.0f,progress))*104),2,c.accent);
  } else {
    // Keep the graph area quiet when capture is pending or unavailable.
    g.drawRect(cx-39,71,79,79,c.muted);
    g.setTextColor(c.muted,c.background);g.drawString(capturing?"Capturing":unavailableTilt(trial.quality),cx,104,1);
  }
  curve(g,rpmCurve,34,184,126,226,c);
}
inline void makeDemo(LaunchMotion::Trace &t,bool reference) {
  t={};t.fused=t.stationaryBias=true;t.quality=LaunchMotion::Quality::Valid;t.count=LaunchMotion::Trace::POINTS;t.number=reference?1:2;
  t.rpm=reference?6000:6500;t.durationMs=reference?400:450;t.gravity[2]=1;
  for(unsigned i=0;i<t.count;++i) {
    auto &p=t.points[i];p.ms=-500+int(i*1250/(t.count-1));
    const float s=std::max(0.0f,std::min(1.0f,p.ms/float(t.durationMs)));
    LaunchMotion::Quaternion q;const float rate[3]={reference?30.0f:45.0f,reference?65.0f:85.0f,reference?10.0f:30.0f};
    q.integrate(rate,s*t.durationMs*.001f);
    p.q[0]=lroundf(q.w*16384);p.q[1]=lroundf(q.x*16384);p.q[2]=lroundf(q.y*16384);p.q[3]=lroundf(q.z*16384);
    if(p.ms>0 && p.ms<int(t.durationMs))for(unsigned j=0;j<3;++j){p.g[j]=lroundf(rate[j]*10);p.a[j]=lroundf(std::sin(s*3.14159265f)*(reference?900:1300)*(j==0?1:.3f));}
  }
  LaunchMotion::recordLevels(t);
}
}
