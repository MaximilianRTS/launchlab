#pragma once
#include <cstdint>
#include "appearance.h"
#include "effect_set.h"
#include "mount_orientation.h"
#include "wear_check.h"

// Navigation never starts/stops measurement. Effects are handled by the sketch
// after the visible state changes; setting writes only happen on Apply.
namespace Controls {
enum class Page:uint8_t {Main,Menu,HistoryMenu,History,Battery,Settings,Edit,NewSession,Tournament,Wipe,WearCheck,LiveTilt,Bluetooth,ForgetPhones};
enum class Effect:uint8_t {None,Recap,BeginEdit,Preview,Apply,CancelEdit,Older,NewSession,Tournament,BeginWipe,ConfirmWipe,CancelWipe,ForgetPhones,ToggleBluetooth};
static constexpr unsigned MENU_COUNT=9;
// Values are stable (HIL `!set N`, saved wake navigation); the screen shows
// them in ORDER. Shake to wake (0.20.2) is listed right after Sleep after.
enum class Setting:uint8_t {Sleep,Rpm,Brightness,Flip,Sensor,Theme,BestEffect,Celebrate,Sound,Mount,ShakeWake,Bluetooth,WipeAll};
// Bluetooth keeps its value and editor (the protocol's id 11, HIL) but has no row here:
// it has its own Menu page. ROWS is the list's length; SETTING_COUNT stands for Back.
static constexpr unsigned EDITABLE_SETTING_COUNT=12,SETTING_COUNT=13,ROWS=12;
// The Settings list, five rows a page: general, appearance, then Mounting,
// Wipe all and Back (SETTING_COUNT, after the last).
static constexpr Setting ORDER[ROWS]={Setting::Sleep,Setting::ShakeWake,Setting::Rpm,Setting::Brightness,Setting::Sensor,
  Setting::Flip,Setting::Theme,Setting::BestEffect,Setting::Celebrate,Setting::Sound,Setting::Mount,Setting::WipeAll};
// A row's place in ORDER (Back, SETTING_COUNT, is last), and the row after it.
constexpr unsigned position(unsigned setting) {
  for(unsigned i=0;i<ROWS;++i)if(unsigned(ORDER[i])==setting)return i;
  return ROWS;
}
constexpr unsigned rowAt(unsigned position){return position<ROWS?unsigned(ORDER[position]):SETTING_COUNT;}
constexpr unsigned nextRow(unsigned setting){return rowAt((position(setting)+1)%(ROWS+1));}
struct State {
  Page page=Page::Main;
  uint8_t menu=0,history=0,setting=0,draft=0;
  // Counts openings of Live tilt, each of which takes a new heading reference.
  uint8_t liveOpened=0;
  bool startSession=false,forgetChosen=false;
  // The Bluetooth page's row: 0 On/Off, 1 Forget phones, 2 Back.
  uint8_t bluetoothRow=0;
  // Valid custom theme slots (bit i: theme THEMES+i), kept current by the
  // sketch; Theme cycles through the built-ins and then these.
  uint8_t customThemes=0;
  // The effect set (effect_set.h), kept current by the sketch; Best effect
  // cycles through Random, these and Off.
  EffectSet::Mask offeredEffects=EffectSet::DEFAULT_MASK;
  WearCheck::Session wear;
  bool autoRecap()const{return page==Page::Main;}
  Effect tapA() {
    switch(page) {
      case Page::Main:return Effect::Recap;
      case Page::Menu:
        switch(menu) {
          case 0:page=Page::HistoryMenu;break;
          case 1:page=Page::NewSession;startSession=false;break;
          case 2:page=Page::Battery;break;
          case 3:page=Page::Settings;break;
          case 4:page=Page::Tournament;return Effect::Tournament;
          case 5:page=Page::WearCheck;wear.reset();break;
          case 6:page=Page::LiveTilt;++liveOpened;break;
          case 7:page=Page::Bluetooth;bluetoothRow=0;break;
          default:page=Page::Main;break;
        }break;
      case Page::HistoryMenu:page=history<3?Page::History:Page::Menu;break;
      case Page::History:page=Page::HistoryMenu;break;
      case Page::Battery:page=Page::Menu;break;
      case Page::Settings:
        if(setting==unsigned(Setting::WipeAll)){page=Page::Wipe;return Effect::BeginWipe;}
        if(setting<EDITABLE_SETTING_COUNT){page=Page::Edit;return Effect::BeginEdit;}
        page=Page::Menu;break;
      case Page::Edit:return Effect::Apply;
      case Page::NewSession:page=Page::Menu;return startSession?Effect::NewSession:Effect::None;
      case Page::Tournament:break;
      case Page::Wipe:return Effect::ConfirmWipe;
      case Page::WearCheck:wear.reset();break;
      case Page::LiveTilt:++liveOpened;break;  // As it is now counts as straight.
      case Page::Bluetooth:
        if(bluetoothRow==0)return Effect::ToggleBluetooth;
        if(bluetoothRow==1){page=Page::ForgetPhones;forgetChosen=false;}
        else page=Page::Menu;
        break;
      case Page::ForgetPhones:page=Page::Bluetooth;return forgetChosen?Effect::ForgetPhones:Effect::None;
    }
    return Effect::None;
  }
  Effect tapB() {
    switch(page) {
      case Page::Main:page=Page::Menu;menu=0;break;
      case Page::Menu:menu=(menu+1)%MENU_COUNT;break;
      case Page::HistoryMenu:history=(history+1)%4;break;
      case Page::History:return Effect::Older;
      case Page::Settings:setting=uint8_t(nextRow(setting));break;
      case Page::Edit:
        switch(Setting(setting)) {
          case Setting::Sleep:draft=draft>=10?1:draft+1;break;
          case Setting::Rpm:case Setting::Flip:case Setting::Sensor:case Setting::ShakeWake:case Setting::Bluetooth:draft=!draft;break;
          case Setting::Mount:draft=(draft+1)%MountOrientation::MODES;break;
          case Setting::Brightness:draft=draft>=100?10:draft+10;break;
          case Setting::Theme:
            do draft=(draft+1)%Appearance::ALL_THEMES;
            while(draft>=Appearance::THEMES && !((customThemes>>(draft-Appearance::THEMES))&1u));
            break;
          case Setting::BestEffect:draft=uint8_t(EffectSet::next(Appearance::Effect(draft),offeredEffects));break;
          case Setting::Celebrate:draft=(draft+1)%3;break;
          case Setting::Sound:draft=(draft+1)%3;break;
          case Setting::WipeAll:return Effect::None;
        }return Effect::Preview;
      case Page::NewSession:startSession=!startSession;break;
      case Page::Tournament:return Effect::Tournament;
      case Page::Battery:break;
      case Page::Wipe:return Effect::CancelWipe;
      case Page::WearCheck:break;
      case Page::LiveTilt:page=Page::Menu;break;
      case Page::Bluetooth:bluetoothRow=(bluetoothRow+1)%3;break;
      case Page::ForgetPhones:forgetChosen=!forgetChosen;break;
    }
    return Effect::None;
  }
  Effect holdB(bool recap=false) {
    switch(page) {
      case Page::Edit:page=Page::Settings;return Effect::CancelEdit;
      case Page::History:page=Page::HistoryMenu;break;
      case Page::HistoryMenu:case Page::Battery:case Page::Settings:case Page::NewSession:case Page::WearCheck:case Page::LiveTilt:case Page::Bluetooth:page=Page::Menu;break;
      case Page::ForgetPhones:page=Page::Bluetooth;break;
      case Page::Menu:page=Page::Main;break;
      case Page::Main:return recap?Effect::Recap:Effect::None;
      case Page::Tournament:break;
      case Page::Wipe:return Effect::CancelWipe;
    }
    return Effect::None;
  }
  Effect holdA(bool recap=false) {
    if(page==Page::Tournament){page=Page::Main;return Effect::Tournament;}
    if(page==Page::Main && !recap){page=Page::Tournament;return Effect::Tournament;}
    return Effect::None;
  }
};
// Gate physical clicks separately from library event details. A consumed hold
// cannot trigger an unrelated click on release; a new press clears the latch.
struct Gesture {
  bool consumed=false;
  bool click(bool pressed,bool holding,bool clicked) {
    if(pressed)consumed=false;
    if(holding)consumed=true;
    return clicked && !consumed;
  }
};
// Saved navigation layouts. 0.10: five settings, setting in 3 bits.
// 0.11: seven settings, setting in 3 bits, view at bit 12. 0.12: eight
// settings in 4 bits (9-12), view at bit 13. 0.13 adds Mount; distinguish
// its schema so the old Back row still opens Back instead of the new editor.
// Adding Wipe uses another schema and never resumes a destructive dialog.
// 0.15 inserts Celebrate after Best effect: older rows from Sound on move
// down one. 0.20.2 adds Shake to wake as value 10: Wipe all and Back move
// down one. 0.20.3 inserts Wear check at menu row 5: an older Back row (5)
// moves down one. Wear check itself wakes on its menu row, so no page value
// after Wipe is ever saved. Live tilt inserts at menu row 6: Back (6) moves down
// one, and it too wakes on its menu row. A later layout had Forget phones as
// Settings value 13, Back at 14. Then the Bluetooth page takes menu row 7 with
// Bluetooth and Forget phones out of Settings: nine rows need a fourth bit, bit 14
// of this layout's magic alone. It wakes on its menu row.
static constexpr uint32_t LEGACY_WAKE_MAGIC=0x4e560000,WAKE7_MAGIC=0x4e570000,
  WAKE8_MAGIC=0x4e580000,WAKE9_MAGIC=0x4e590000,WAKE10_MAGIC=0x4e5a0000,WAKE11_MAGIC=0x4e5b0000,
  WAKE12_MAGIC=0x4e5c0000,WAKE13_MAGIC=0x4e5d0000,WAKE14_MAGIC=0x4e5e0000,WAKE15_MAGIC=0x4e5f0000,WAKE16_MAGIC=0x4e600000,WAKE_MAGIC=0x4e610000;
inline uint32_t encodeWake(State state,uint8_t tournamentView) {
  if(state.page==Page::Edit || state.page==Page::Wipe)state.page=Page::Settings;
  if(state.page==Page::WearCheck){state.page=Page::Menu;state.menu=5;}
  if(state.page==Page::LiveTilt){state.page=Page::Menu;state.menu=6;}
  if(state.page==Page::Bluetooth || state.page==Page::ForgetPhones){state.page=Page::Menu;state.menu=7;}
  return WAKE_MAGIC|uint32_t(state.page)|(uint32_t(state.menu&7)<<4)|(uint32_t(state.menu>>3)<<14)|
    (uint32_t(state.history)<<7)|(uint32_t(state.setting)<<9)|(uint32_t(tournamentView)<<13);
}
inline bool decodeWake(uint32_t value,State &out,uint8_t &tournamentView) {
  State state;state.page=Page(value&15);state.history=(value>>7)&3;
  const bool current=(value&0xffff8000)==WAKE_MAGIC;
  state.menu=((value>>4)&7)|(current?((value>>14)&1)<<3:0);
  // Each layout's Back row is the one after its last setting.
  unsigned setting=0,back=0;
  const auto wideMagic=current?WAKE_MAGIC:value&0xffffc000;
  if(wideMagic==WAKE_MAGIC || wideMagic==WAKE16_MAGIC || wideMagic==WAKE15_MAGIC || wideMagic==WAKE14_MAGIC || wideMagic==WAKE13_MAGIC || wideMagic==WAKE12_MAGIC || wideMagic==WAKE11_MAGIC || wideMagic==WAKE10_MAGIC || wideMagic==WAKE9_MAGIC || wideMagic==WAKE8_MAGIC) {
    setting=(value>>9)&15;tournamentView=(value>>13)&1;
    back=wideMagic==WAKE8_MAGIC?8:wideMagic==WAKE9_MAGIC?9:wideMagic==WAKE10_MAGIC?10:wideMagic==WAKE11_MAGIC?11:wideMagic==WAKE_MAGIC?SETTING_COUNT:wideMagic==WAKE16_MAGIC?14:wideMagic==WAKE15_MAGIC || wideMagic==WAKE14_MAGIC?13:12;
  }
  else {
    const auto magic=value&0xffffe000;
    if(magic!=WAKE7_MAGIC && magic!=LEGACY_WAKE_MAGIC)return false;
    back=magic==LEGACY_WAKE_MAGIC?5:7;
    setting=(value>>9)&7;tournamentView=(value>>12)&1;
  }
  if(setting>back)return false;
  if(setting==back)setting=SETTING_COUNT;
  else if(wideMagic==WAKE12_MAGIC || wideMagic==WAKE13_MAGIC) {
    // Before Bluetooth (ble): Wipe all was 11.
    if(setting==11)setting=unsigned(Setting::WipeAll);
  }
  else if(wideMagic==WAKE16_MAGIC) {
    // Forget phones, 13 then, has left Settings too.
    if(setting==13)setting=unsigned(Setting::WipeAll);
  }
  else if(wideMagic!=WAKE_MAGIC && wideMagic!=WAKE15_MAGIC && wideMagic!=WAKE14_MAGIC) {
    // In the 0.15-0.20.1 numbering (WAKE11): Celebrate is 7, Wipe all 10.
    if(wideMagic!=WAKE11_MAGIC && setting>=7)++setting;
    if(setting==10)setting=unsigned(Setting::WipeAll);
  }
  // Bluetooth has no Settings row now: the row beside where it was.
  if(setting==unsigned(Setting::Bluetooth))setting=unsigned(Setting::WipeAll);
  state.setting=uint8_t(setting);
  const bool eightRows=wideMagic==WAKE16_MAGIC || wideMagic==WAKE15_MAGIC;
  const bool sevenRows=wideMagic==WAKE14_MAGIC || wideMagic==WAKE13_MAGIC;
  const bool sixRows=wideMagic!=WAKE_MAGIC && !eightRows && !sevenRows;
  if(uint8_t(state.page)>uint8_t(Page::Wipe) || state.page==Page::Edit || state.page==Page::Wipe ||
     state.menu>=(sixRows?6:sevenRows?7:eightRows?8:MENU_COUNT) || state.setting>SETTING_COUNT)return false;
  // Each older layout's Back row is the new Back.
  if((sixRows && state.menu==5) || (sevenRows && state.menu==6) || (eightRows && state.menu==7))state.menu=MENU_COUNT-1;
  out=state;return true;
}
}
