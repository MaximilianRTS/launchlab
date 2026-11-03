#include "mount_orientation_setting.h"
#include <cassert>
#include <iostream>

int main() {
  Preferences prefs;prefs.begin("launchlab-mini",false);
  const uint8_t prior[]={4,8,15,16,23,42};
  for(const char *key:{"sleep_min","bright_pct","rpm_method","sensor_profile","disp_flip","wake_nav","history0","reference0"})
    prefs.putBytes(key,prior,sizeof(prior));
  const auto existing=Preferences::data;
  using MountOrientation::Mode;
  Mode mode=Mode::Sidecar;
  assert(MountOrientationSetting::load(prefs,mode) && mode==Mode::Top && Preferences::data==existing);
  for(Mode selected:{Mode::Sidecar,Mode::Top,Mode::Top90L,Mode::Top90R,Mode::Sidecar}) {
    assert(MountOrientationSetting::save(prefs,selected));
    Preferences reboot;reboot.begin("launchlab-mini",false);
    assert(MountOrientationSetting::load(reboot,mode) && mode==selected);
    for(const auto &entry:existing)assert(Preferences::data.at(entry.first)==entry.second);
  }
  const auto good=Preferences::data;
  assert(!MountOrientationSetting::save(prefs,Mode(4)) && Preferences::data==good);
  Preferences::failWrite=true;
  assert(!MountOrientationSetting::save(prefs,Mode::Top) && Preferences::data==good);
  Preferences::failWrite=false;Preferences::shortRead=true;
  assert(!MountOrientationSetting::load(prefs,mode) && mode==Mode::Top && Preferences::data==good);
  Preferences::shortRead=false;
  const auto key=std::string("launchlab-mini")+MountOrientationSetting::KEY;
  for(const std::vector<unsigned char> bad:std::initializer_list<std::vector<unsigned char>>{
      {},{0x4d,1,1},{0x4c,1,1,uint8_t(0x4c^1^1)},
      {0x4d,2,1,uint8_t(0x4d^2^1)},{0x4d,1,4,uint8_t(0x4d^1^4)},{0x4d,1,1,0}}) {
    Preferences::data[key]=bad;const auto retained=Preferences::data;
    mode=Mode::Sidecar;
    assert(!MountOrientationSetting::load(prefs,mode) && mode==Mode::Top && Preferences::data==retained);
  }
  Preferences::data=good;Preferences::zeroLengthKeys.insert(key);
  assert(!MountOrientationSetting::load(prefs,mode) && mode==Mode::Top && Preferences::data==good);
  std::cout<<"PASS: Top migration without writes, Sidecar and Top 90 L/R survive restart, unrelated settings/history untouched, corrupt/failed mounting records retained safely\n";
}
