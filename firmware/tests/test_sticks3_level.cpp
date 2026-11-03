#include "../LaunchLabMini/level_indicator.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using L=StickS3Level;
static bool near(float a,float b){return std::fabs(a-b)<0.01f;}
int main() {
  L absent;assert(!absent.reading(1000).valid());
  // Independent unit-gravity vectors establish the angle, including face down.
  for(float z:{-1.0f,1.0f}) {
    L flat;flat.feed(0,0,z,0);auto r=flat.reading(0);
    assert(r.valid() && near(r.degrees,0) && near(r.right,0) && near(r.down,0));
  }
  for(float sign:{-1.0f,1.0f}) {
    L tilted;tilted.feed(sign*0.5f,0,std::sqrt(0.75f),1000);
    auto r=tilted.reading(1000);assert(r.valid() && near(r.degrees,30));
    assert(near(r.right,sign) && near(r.down,0));
    auto flipOff=L::forDisplay(r,false),flipOn=L::forDisplay(r,true);
    assert(flipOff.valid() && near(flipOff.degrees,30) && near(flipOff.right,-sign) && near(flipOff.down,0));
    assert(flipOn.valid() && near(flipOn.degrees,30) && near(flipOn.right,sign) && near(flipOn.down,0));
    assert(near(r.right,sign)); // Drawing does not mutate recorded/native data.
    // Turned on the launcher, the dot stays true to the glass: launcher +X is the
    // stick's screen-top for Top 90 R and its screen-bottom for Top 90 L.
    {
      const auto cw=L::forDisplay(r,true,1),ccw=L::forDisplay(r,true,-1);
      assert(near(cw.degrees,30) && near(cw.right,0) && near(cw.down,-sign));
      assert(near(ccw.degrees,30) && near(ccw.right,0) && near(ccw.down,sign));
    }
    L other;other.feed(0,sign*0.5f,std::sqrt(0.75f),1000);
    r=other.reading(1000);assert(near(r.right,0) && near(r.down,-sign));
    flipOff=L::forDisplay(r,false);flipOn=L::forDisplay(r,true);
    assert(flipOff.valid() && near(flipOff.degrees,30) && near(flipOff.right,0) && near(flipOff.down,sign));
    assert(flipOn.valid() && near(flipOn.degrees,30) && near(flipOn.right,0) && near(flipOn.down,-sign));
  }
  L diagonal;diagonal.feed(0.5f,0.5f,std::sqrt(0.5f),1);
  auto r=diagonal.reading(1);assert(near(r.degrees,45));
  assert(near(std::hypot(r.right,r.down),1)); // Dot stays inside the rim.
  assert(diagonal.reading(251).valid() && !diagonal.reading(252).valid());
  // Bad data must hide the last good bubble, then recover without stale filtering.
  for(float bad:{0.0f,2.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    L sensor;sensor.feed(0,0,1,1000);sensor.feed(bad,0,0,1020);
    assert(sensor.reading(1020).state==L::State::Moving);
    sensor.feed(0,0,1,1040);assert(sensor.reading(1040).valid() && near(sensor.reading(1040).degrees,0));
  }
  // Smoothing settles to a known angle and resets after a real freshness gap.
  L smooth;smooth.feed(0,0,1,1000);smooth.feed(0.5f,0,std::sqrt(0.75f),1020);
  assert(smooth.reading(1020).degrees>0 && smooth.reading(1020).degrees<5);
  for(uint32_t ms=1040;ms<4000;ms+=20)smooth.feed(0.5f,0,std::sqrt(0.75f),ms);
  assert(near(smooth.reading(3980).degrees,30));
  smooth.feed(0,0,1,5000);assert(near(smooth.reading(5000).degrees,0));
  L wrap;wrap.feed(0,0,1,UINT32_MAX-100);
  assert(wrap.reading(50).valid() && !wrap.reading(200).valid());
  std::cout<<"PASS: level geometry, bubble limits, fresh/invalid data, smoothing, recovery and clock wrap\n";
}
