#include "../LaunchLabMini/ble_diagnostics.h"
#include <cassert>
#include <cmath>
#include <iostream>
static uint16_t get16(const uint8_t *p){uint16_t v;memcpy(&v,p,2);return v;}
static uint32_t get32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
int main() {
  using namespace BleDiagnostics;
  Values v;
  v.state=2;v.minMark=80;v.lastMark=304.6f;v.medianMark=305;v.noise=12.4f;v.samplesPerSecond=49992;
  v.signalMin=3515;v.signalMax=3876;
  v.weak=26;v.slow=414;v.shape=8492;v.inconsistent=4801;v.dataLoss=3;v.maxReadGapUs=22346;
  uint8_t out[BYTES+1];out[BYTES]=0xA5;
  // The layout in docs/BLE_PROTOCOL.md, and no byte past it.
  assert(encode(v,out)==BYTES);assert(out[BYTES]==0xA5);
  assert(out[0]==2);
  assert(get16(out+1)==80);assert(get16(out+3)==305);assert(get16(out+5)==305);assert(get16(out+7)==12);
  assert(get16(out+9)==3515);assert(get16(out+11)==3876);assert(get16(out+13)==49992);
  assert(get32(out+15)==26);assert(get32(out+19)==414);assert(get32(out+23)==8492);assert(get32(out+27)==4801);
  assert(get32(out+31)==3);assert(get32(out+35)==22346);
  // Nothing wraps: no reading yet, or one out of range, stays in 16 bits.
  assert(rounded16(0)==0);assert(rounded16(-5)==0);assert(rounded16(NAN)==0);assert(rounded16(70000)==65535);
  assert(clamped16(70000)==65535);assert(clamped16(65535)==65535);
  std::cout<<"ble diagnostics tests passed\n";
}
