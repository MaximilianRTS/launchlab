#include "../LaunchLabMini/ble_upload.h"
#include "../LaunchLabMini/ble_wear.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace BleUpload;
  Buffer b;const uint8_t bytes[4]={1,2,3,4};
  // Nothing without a begin, and only known kinds and lengths begin.
  assert(!b.add(0,bytes,4));assert(!b.complete());
  assert(!b.begin(3,4));assert(!b.begin(Theme,0));assert(!b.begin(Theme,MAX_BYTES+1));
  assert(b.begin(Milestones,MAX_BYTES));
  // In order, within the length.
  assert(b.begin(Theme,6));
  assert(!b.add(2,bytes,2));assert(b.add(0,bytes,4));assert(!b.complete());
  assert(!b.add(4,bytes,3));assert(!b.add(4,bytes,0));
  assert(b.add(4,bytes,2));assert(b.complete());assert(b.length()==6);
  assert(b.kind()==Theme);assert(b.data()[4]==1 && b.data()[5]==2);
  // Past the end is refused; a new begin starts over.
  assert(!b.add(6,bytes,1));
  assert(b.begin(Theme,1));assert(!b.complete());
  b.reset();assert(!b.complete());assert(!b.kind());
  // The result line goes back with a status and no line end.
  uint8_t out[RESULT_CHARS+2];
  assert(encodeResult("THEME_OK op=put slot=0\n",out)==2+22);assert(out[0]==0 && out[1]==22 && out[2+21]=='0');
  encodeResult("MILESTONE_ERROR op=set reason=busy written=0\n",out);assert(out[0]==2);
  encodeResult("THEME_ERROR reason=invalid_request written=0\n",out);assert(out[0]==1);
  // No result at all is not success.
  assert(encodeResult("",out)==2);assert(out[0]==1 && out[1]==0);

  // Wear check's layout in docs/BLE_PROTOCOL.md.
  BleWear::Values v;v.active=1;v.stage=2;v.step=3;v.alignment=4;v.detectorReady=1;v.outcome=2;v.mountHint=1;v.tiltValid=1;
  v.tilt=1.5f;v.lastRpm=2.5f;v.count[2]=3;v.rpm[2][2]=9.5f;v.mean=10.5f;v.medianMean=11.5f;v.stageMean[2]=12.5f;
  uint8_t w[BleWear::BYTES+1];w[BleWear::BYTES]=0xA5;
  assert(BleWear::encode(v,w)==BleWear::BYTES);assert(w[BleWear::BYTES]==0xA5);
  auto f=[&w](size_t at){float x;memcpy(&x,w+at,4);return x;};
  assert(w[0]==1 && w[1]==2 && w[2]==3 && w[3]==4 && w[4]==1 && w[5]==2 && w[6]==1 && w[7]==1);
  assert(f(8)==1.5f);assert(f(12)==2.5f);assert(w[18]==3);
  assert(f(19+32)==9.5f);assert(f(55)==10.5f);assert(f(59)==11.5f);assert(f(71)==12.5f);
  std::cout<<"ble upload tests passed\n";
}
