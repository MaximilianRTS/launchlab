#include "controls.h"
#include <cassert>
#include <iostream>
using namespace Controls;
int main() {
  State s;assert(s.autoRecap() && s.tapA()==Effect::Recap);
  assert(s.holdB(true)==Effect::Recap && s.holdB()==Effect::None);
  s.tapB();assert(s.page==Page::Menu && s.menu==0 && !s.autoRecap());
  s.tapA();assert(s.page==Page::HistoryMenu);
  for(unsigned view=0;view<3;++view) {
    s.history=view;s.tapA();assert(s.page==Page::History && s.tapB()==Effect::Older);
    s.holdB();assert(s.page==Page::HistoryMenu && s.history==view);
  }
  s.history=3;s.tapA();assert(s.page==Page::Menu);
  s.menu=1;s.tapA();assert(s.page==Page::NewSession && !s.startSession);
  assert(s.tapA()==Effect::None && s.page==Page::Menu);
  s.tapA();s.tapB();assert(s.tapA()==Effect::NewSession && s.page==Page::Menu);
  s.menu=3;s.tapA();assert(s.page==Page::Settings);
  for(unsigned row=0;row<EDITABLE_SETTING_COUNT;++row) {
    s.setting=row;assert(s.tapA()==Effect::BeginEdit && s.page==Page::Edit);
    assert(s.tapA()==Effect::Apply && s.page==Page::Edit); // Backend must confirm success.
    assert(s.holdA()==Effect::None);assert(s.holdB()==Effect::CancelEdit && s.page==Page::Settings);
  }
  s.page=Page::Settings;s.setting=unsigned(Setting::WipeAll);
  assert(s.tapA()==Effect::BeginWipe && s.page==Page::Wipe);
  assert(s.tapA()==Effect::ConfirmWipe && s.holdA()==Effect::None);
  assert(s.tapB()==Effect::CancelWipe && s.holdB()==Effect::CancelWipe);
  s.page=Page::Settings; // The sketch consumes CancelWipe before navigating.
  s.setting=SETTING_COUNT;s.tapA();assert(s.page==Page::Menu);
  s.menu=5;s.tapA();assert(s.page==Page::WearCheck && !s.autoRecap() && s.wear.totalAccepted()==0);
  assert(s.wear.observePull(1,true,3250,WearCheck::Tilt(true,1.0f,0.0f,0.0f))==WearCheck::Outcome::Accepted && s.wear.totalAccepted()==1);
  assert(s.tapB()==Effect::None && s.page==Page::WearCheck && s.wear.totalAccepted()==1);
  assert(s.tapA()==Effect::None && s.page==Page::WearCheck && s.wear.totalAccepted()==0);
  assert(s.holdB()==Effect::None && s.page==Page::Menu && s.menu==5);
  // Live tilt opens from its row; A or hold B returns there, B does nothing.
  {const auto opened=s.liveOpened;s.menu=6;s.tapA();assert(s.page==Page::LiveTilt && !s.autoRecap() && s.liveOpened==uint8_t(opened+1));}
  // A makes the launcher's direction straight, as a fresh opening does; B goes back.
  {const auto opened=s.liveOpened;assert(s.tapA()==Effect::None && s.page==Page::LiveTilt && s.liveOpened==uint8_t(opened+1));}
  assert(s.tapB()==Effect::None && s.page==Page::Menu && s.menu==6);
  s.tapA();assert(s.page==Page::LiveTilt);
  assert(s.holdB()==Effect::None && s.page==Page::Menu && s.menu==6);
  s.menu=8;s.tapA();assert(s.page==Page::Main);s.page=Page::Menu;s.menu=5;
  s.holdB();assert(s.page==Page::Main);
  assert(s.holdA(true)==Effect::None && s.page==Page::Main);
  assert(s.holdA()==Effect::Tournament && s.page==Page::Tournament && !s.autoRecap());
  assert(s.tapA()==Effect::None && s.holdB()==Effect::None && s.page==Page::Tournament);
  assert(s.tapB()==Effect::Tournament && s.page==Page::Tournament);
  assert(s.holdA()==Effect::Tournament && s.page==Page::Main);
  // Best effect cycles Random, the offered effects (the default set: the 16
  // of 0.16) and Off.
  for(unsigned row=0;row<EDITABLE_SETTING_COUNT;++row) {
    s.page=Page::Edit;s.setting=row;s.draft=row==0?3:row==2?40:0;
    const unsigned original=s.draft,cycle=row==0 || row==2?10:row==5?Appearance::THEMES:row==6?EffectSet::count(s.offeredEffects)+2:row==7 || row==8?3:row==unsigned(Setting::Mount)?MountOrientation::MODES:2;
    for(unsigned i=0;i<cycle;++i)assert(s.tapB()==Effect::Preview);
    assert(s.draft==original);
  }
  // Every theme is reachable exactly once from every saved starting choice.
  for(unsigned start=0;start<Appearance::THEMES;++start) {
    s.page=Page::Edit;s.setting=unsigned(Setting::Theme);s.draft=start;
    unsigned visited=0;
    for(unsigned i=0;i<Appearance::THEMES;++i){visited|=1u<<s.draft;assert(s.tapB()==Effect::Preview);}
    assert(s.draft==start && visited==(1u<<Appearance::THEMES)-1);
  }
  // Hold/release must never execute both actions, including a synthetic
  // library release click and a new short press immediately afterwards.
  Gesture gesture;assert(!gesture.click(true,false,false));
  assert(gesture.click(false,false,true));
  assert(!gesture.click(true,false,false));assert(!gesture.click(false,true,false));
  assert(!gesture.click(false,false,true));assert(!gesture.click(true,false,false));
  assert(gesture.click(false,false,true));
  for(unsigned page=0;page<=unsigned(Page::WearCheck);++page)
    for(unsigned menu=0;menu<MENU_COUNT;++menu)for(unsigned setting=0;setting<=SETTING_COUNT;++setting)for(uint8_t view=0;view<2;++view) {
      State before;before.page=Page(page);before.menu=menu;before.setting=setting;before.history=2;
      State after;uint8_t saved=99;
      assert(decodeWake(encodeWake(before,view),after,saved));
      // Wear check wakes on its Menu row: a fresh check, and no page value
      // beyond Wipe is ever saved.
      const bool wear=before.page==Page::WearCheck;
      assert(after.page==((before.page==Page::Edit || before.page==Page::Wipe)?Page::Settings:wear?Page::Menu:before.page));
      assert(after.menu==(wear?5:menu) && after.setting==(setting==unsigned(Setting::Bluetooth)?unsigned(Setting::WipeAll):setting) && after.history==2 && saved==view);
    }
  State out;uint8_t view=0;
  // 0.20.2 and earlier menus had six rows with Back at 5; Wear check (0.20.3)
  // took row 5, so an older Back row still opens Back.
  for(uint32_t magic:{WAKE12_MAGIC,WAKE11_MAGIC,WAKE10_MAGIC,WAKE9_MAGIC,WAKE8_MAGIC,WAKE7_MAGIC,LEGACY_WAKE_MAGIC}) {
    assert(decodeWake(magic|uint32_t(Page::Menu)|(5u<<4),out,view) && out.page==Page::Menu && out.menu==MENU_COUNT-1);
    assert(decodeWake(magic|uint32_t(Page::Menu)|(4u<<4),out,view) && out.menu==4);
    assert(!decodeWake(magic|uint32_t(Page::Menu)|(6u<<4),out,view));
  }
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Menu)|(5u<<4),out,view) && out.menu==5);
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Menu)|(6u<<4),out,view) && out.menu==6);
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Menu)|(7u<<4),out,view) && out.menu==7);
  // Before Live tilt, Back was row 6; the rows above it keep their places.
  for(uint32_t magic:{WAKE14_MAGIC,WAKE13_MAGIC}) {
    assert(decodeWake(magic|uint32_t(Page::Menu)|(6u<<4),out,view) && out.menu==MENU_COUNT-1);
    assert(decodeWake(magic|uint32_t(Page::Menu)|(5u<<4),out,view) && out.menu==5);
    assert(!decodeWake(magic|uint32_t(Page::Menu)|(7u<<4),out,view));
  }
  // Bluetooth left Settings: its old row wakes on the row beside it.
  assert(decodeWake(WAKE14_MAGIC|uint32_t(Page::Settings)|(unsigned(Setting::Bluetooth)<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  // Before Forget phones, Back was 13; Wipe all keeps its value.
  for(uint32_t magic:{WAKE15_MAGIC,WAKE14_MAGIC}) {
    assert(decodeWake(magic|uint32_t(Page::Settings)|(13u<<9),out,view) && out.setting==SETTING_COUNT);
    assert(decodeWake(magic|uint32_t(Page::Settings)|(12u<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
    assert(!decodeWake(magic|uint32_t(Page::Settings)|(14u<<9),out,view));
  }
  // With Forget phones in Settings, it was 13 and Back 14.
  assert(decodeWake(WAKE16_MAGIC|uint32_t(Page::Settings)|(14u<<9),out,view) && out.setting==SETTING_COUNT);
  assert(decodeWake(WAKE16_MAGIC|uint32_t(Page::Settings)|(13u<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  // Eight rows: Back was 7, and rows above it stay.
  for(uint32_t magic:{WAKE16_MAGIC,WAKE15_MAGIC}) {
    assert(decodeWake(magic|uint32_t(Page::Menu)|(7u<<4),out,view) && out.menu==MENU_COUNT-1);
    assert(decodeWake(magic|uint32_t(Page::Menu)|(6u<<4),out,view) && out.menu==6);
    assert(!decodeWake(magic|0x4000u|uint32_t(Page::Menu),out,view));
  }
  // Nine rows: row 8 takes bit 14, this layout's alone.
  {
    State menu;menu.page=Page::Menu;menu.menu=8;uint8_t saved=0;
    const uint32_t code=encodeWake(menu,0);assert(code&0x4000u);
    assert(decodeWake(code,out,saved) && out.page==Page::Menu && out.menu==8);
    menu.menu=7;assert(!(encodeWake(menu,0)&0x4000u) && decodeWake(encodeWake(menu,0),out,saved) && out.menu==7);
    assert(!decodeWake(WAKE_MAGIC|0x4000u|(1u<<4),out,saved)); // Row 9 does not exist.
    assert(!decodeWake(WAKE_MAGIC|0x8000u,out,saved));
  }
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Settings)|(13u<<9),out,view) && out.setting==SETTING_COUNT);
  assert(!decodeWake(WAKE_MAGIC|uint32_t(Page::Settings)|(14u<<9),out,view));
  // The Bluetooth page: from its menu row; B walks On/Off, Forget phones, Back. A
  // toggles at one press. Hold B returns; Forget takes a choice, and comes back here.
  {
    State p;p.page=Page::Menu;p.menu=7;uint8_t saved=0;
    assert(p.tapA()==Effect::None && p.page==Page::Bluetooth && p.bluetoothRow==0);
    assert(p.tapA()==Effect::ToggleBluetooth && p.page==Page::Bluetooth);
    assert(!decodeWake(WAKE_MAGIC|uint32_t(Page::Bluetooth),out,saved) && !decodeWake(WAKE_MAGIC|uint32_t(Page::ForgetPhones),out,saved));
    assert(decodeWake(encodeWake(p,0),out,saved) && out.page==Page::Menu && out.menu==7);
    assert(p.tapB()==Effect::None && p.bluetoothRow==1);
    assert(p.tapA()==Effect::None && p.page==Page::ForgetPhones && !p.forgetChosen);
    assert(decodeWake(encodeWake(p,0),out,saved) && out.page==Page::Menu && out.menu==7);
    assert(p.tapA()==Effect::None && p.page==Page::Bluetooth); // Cancel by default.
    p.tapA();assert(p.tapB()==Effect::None && p.forgetChosen);
    assert(p.holdB()==Effect::None && p.page==Page::Bluetooth); // Hold B never forgets.
    p.tapA();assert(!p.forgetChosen);p.tapB();
    assert(p.tapA()==Effect::ForgetPhones && p.page==Page::Bluetooth);
    assert(p.tapB()==Effect::None && p.bluetoothRow==2);
    assert(p.tapA()==Effect::None && p.page==Page::Menu);
    p.tapA();assert(p.holdB()==Effect::None && p.page==Page::Menu);
    // Neither is a Settings row any more; the protocol's id stays.
    static_assert(position(unsigned(Setting::Bluetooth))==ROWS && unsigned(Setting::Bluetooth)==11);
    // Every row is reached in ORDER, once.
    unsigned row=0,seen=0;do{row=nextRow(row);++seen;}while(row!=0 && seen<40);assert(seen==ROWS+1);
  }
  // Live tilt never wakes on its own page: on its menu row.
  {
    State live;live.page=Page::LiveTilt;uint8_t saved=0;
    assert(decodeWake(encodeWake(live,0),out,saved) && out.page==Page::Menu && out.menu==6);
    assert(!decodeWake(WAKE_MAGIC|uint32_t(Page::LiveTilt),out,saved));
  }
  // 0.20.2 shares this settings numbering.
  assert(decodeWake(WAKE12_MAGIC|uint32_t(Page::Settings)|(10u<<9),out,view) && out.setting==unsigned(Setting::ShakeWake));
  assert(decodeWake(WAKE12_MAGIC|uint32_t(Page::Settings)|(12u<<9)|(1u<<13),out,view) && out.setting==SETTING_COUNT && view==1);
  assert(decodeWake(LEGACY_WAKE_MAGIC|uint32_t(Page::Settings)|(5u<<9),out,view) && out.setting==SETTING_COUNT);
  // 0.11 layout: seven settings, Back at row 7, view at bit 12.
  assert(decodeWake(WAKE7_MAGIC|uint32_t(Page::Settings)|(7u<<9)|(1u<<12),out,view) && out.setting==SETTING_COUNT && view==1);
  assert(decodeWake(WAKE7_MAGIC|uint32_t(Page::Settings)|(6u<<9),out,view) && out.setting==6 && view==0);
  // 0.12 Back must stay Back after inserting Mount at row 8.
  assert(decodeWake(WAKE8_MAGIC|uint32_t(Page::Settings)|(8u<<9)|(1u<<13),out,view) && out.setting==SETTING_COUNT && view==1);
  // Rows from Sound on moved down one when Celebrate was inserted after Best effect.
  assert(decodeWake(WAKE8_MAGIC|uint32_t(Page::Settings)|(7u<<9),out,view) && out.setting==unsigned(Setting::Sound) && view==0);
  assert(decodeWake(WAKE8_MAGIC|uint32_t(Page::Settings)|(6u<<9),out,view) && out.setting==unsigned(Setting::BestEffect));
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Settings)|(9u<<9),out,view) && out.setting==unsigned(Setting::Mount));
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Settings)|(7u<<9),out,view) && out.setting==unsigned(Setting::Celebrate));
  assert(decodeWake(WAKE9_MAGIC|uint32_t(Page::Settings)|(9u<<9)|(1u<<13),out,view) && out.setting==SETTING_COUNT && view==1);
  assert(decodeWake(WAKE9_MAGIC|uint32_t(Page::Settings)|(8u<<9),out,view) && out.setting==unsigned(Setting::Mount));
  // Before Bluetooth: Wipe all at row 11 and Back at row 12; rows before them keep their numbers.
  assert(decodeWake(WAKE13_MAGIC|uint32_t(Page::Settings)|(10u<<9),out,view) && out.setting==unsigned(Setting::ShakeWake));
  assert(decodeWake(WAKE13_MAGIC|uint32_t(Page::Settings)|(11u<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  assert(decodeWake(WAKE13_MAGIC|uint32_t(Page::Settings)|(12u<<9),out,view) && out.setting==SETTING_COUNT);
  assert(decodeWake(WAKE12_MAGIC|uint32_t(Page::Settings)|(11u<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  // Six menu rows only before String wear check: WAKE13 already has seven.
  assert(decodeWake(WAKE13_MAGIC|uint32_t(Page::Menu)|(5u<<4),out,view) && out.menu==5);
  // Bluetooth has its own row and toggles in the editor.
  assert(decodeWake(WAKE_MAGIC|uint32_t(Page::Settings)|(unsigned(Setting::Bluetooth)<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  assert(position(unsigned(Setting::Mount))+1==position(unsigned(Setting::WipeAll)));
  // 0.13.9-0.14: Wipe all at row 9 and Back at row 10.
  assert(decodeWake(WAKE10_MAGIC|uint32_t(Page::Settings)|(7u<<9),out,view) && out.setting==unsigned(Setting::Sound));
  assert(decodeWake(WAKE10_MAGIC|uint32_t(Page::Settings)|(8u<<9),out,view) && out.setting==unsigned(Setting::Mount));
  assert(decodeWake(WAKE10_MAGIC|uint32_t(Page::Settings)|(9u<<9),out,view) && out.setting==unsigned(Setting::WipeAll));
  assert(decodeWake(WAKE10_MAGIC|uint32_t(Page::Settings)|(10u<<9)|(1u<<13),out,view) && out.setting==SETTING_COUNT && view==1);
  assert(decodeWake(WAKE10_MAGIC|uint32_t(Page::Settings)|(5u<<9),out,view) && out.setting==unsigned(Setting::Theme));
  for(uint32_t corrupt:{0u,WAKE_MAGIC|15u,WAKE_MAGIC|uint32_t(Page::Edit),WAKE14_MAGIC|(7u<<4),LEGACY_WAKE_MAGIC|(7u<<9),
                        WAKE16_MAGIC|0x4000u,WAKE15_MAGIC|(14u<<9),WAKE_MAGIC|(15u<<9),WAKE13_MAGIC|(13u<<9),WAKE12_MAGIC|(13u<<9),WAKE11_MAGIC|(12u<<9),WAKE10_MAGIC|(11u<<9),WAKE8_MAGIC|(9u<<9),WAKE9_MAGIC|(10u<<9),
                        WAKE_MAGIC|uint32_t(Page::Wipe),WAKE9_MAGIC|uint32_t(Page::Wipe),WAKE7_MAGIC|0x2000u,
                        WAKE_MAGIC|uint32_t(Page::WearCheck),WAKE12_MAGIC|uint32_t(Page::WearCheck),WAKE12_MAGIC|(7u<<4)})
    assert(!decodeWake(corrupt,out,view));
  std::cout<<"PASS: consistent actions, visible menu/back, deliberate sessions, staged settings, tournament lock, hold release, wake round trips and corruption\n";
}
