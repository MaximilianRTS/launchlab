#pragma once
#include <array>
#include <cstdint>

// The words the screen draws, by key, so the UI can be shown in more than one
// language (I18N). English is the reference: every key has its English text
// here, the tests and HIL run in English, and a language without a key falls
// back to it. Only screen text goes through this table: Serial, HIL, BLE
// payloads, NVS keys and backups keep their own fixed English names (for
// example MountOrientation::label(), Acquisition's stateName() and
// ControlsUI::protocolValue()), so a language never changes a protocol.
//
// Each text keeps the width and the font the English one is drawn in: a
// translation has to fit the same place (Font 1 is 6 px a glyph; the body
// keeps x 10..125, the footer the whole 135 px).
//
// The tables live in flash: constexpr arrays of const pointers to literals.
namespace UiText {

// X(key, English text). Grouped by screen; printf formats keep their
// arguments in the order the comment names.
#define LAUNCHLAB_UI_TEXT(X) \
  /* Shared words and footers */ \
  X(Back,"Back") X(On,"On") X(Off,"Off") X(Cancel,"Cancel") X(Menu,"Menu") \
  X(FooterOpenNext,"A Open  B Next") X(FooterBackNext,"A Back  B Next") X(FooterEditNext,"A Edit  B Next") \
  X(FooterChooseNext,"A Choose  B Next") X(FooterApplyNext,"A Apply  B Next") X(FooterBack,"A Back") X(FooterDone,"A Done") \
  X(FooterTurnOffNext,"A Turn off  B Next") X(FooterTurnOnNext,"A Turn on  B Next") \
  X(FooterViewExit,"B View  Hold A Exit") X(FooterMainMenu,"A Main  B Menu") \
  X(FooterWonLost,"A Won  B Lost") X(FooterRecapMenu,"A Recap  B Menu") \
  X(HoldEnterTournament,"Enter tournament") X(HoldExitTournament,"Exit tournament") \
  X(HoldSkipBattle,"Skip battle") X(HoldOk,"OK") X(HoldEnterAnalysis,"Enter analysis") X(HoldExitAnalysis,"Exit analysis") \
  X(HoldCancelChange,"Cancel change") \
  /* Menu rows (Controls::MenuRow) */ \
  X(MenuHistory,"History") X(MenuNewSession,"New session") X(MenuTournament,"Tournament") X(MenuBattles,"Battles") \
  X(MenuWearCheck,"String wear check") X(MenuAnalysis,"Analysis") X(MenuDevice,"Device") X(MenuBluetooth,"Bluetooth") \
  X(MenuSettings,"Settings") \
  /* History menu rows */ \
  X(HistoryLaunches,"Launches") X(HistoryPullTrend,"Pull trend") X(HistorySessionTrend,"Session trend") \
  /* Settings rows (Controls::Setting) */ \
  X(SettingSleep,"Sleep after") X(SettingRpm,"RPM method") X(SettingBrightness,"Brightness") X(SettingFlip,"Flip display") \
  X(SettingSensor,"Sensor profile") X(SettingTheme,"Theme") X(SettingBestEffect,"Best effect") X(SettingCelebrate,"Celebrate") \
  X(SettingSound,"Sound") X(SettingMount,"Mounting") X(SettingShakeWake,"Shake to wake") X(SettingWipeAll,"Wipe all") \
  X(SettingAbout,"About") X(SettingBatteryIcon,"Battery icon") X(SettingBluetooth,"Bluetooth") X(SettingMinRpm,"Min RPM") \
  /* Settings values */ \
  X(ValueMinutes,"%u min") /* minutes */ \
  X(ValueRpmThreeTurn,"3-turn average") X(ValueRpmOneTurn,"1-turn peak") \
  X(ValueSensorTcrt,"TCRT5000") X(ValueSensorStandard,"QRE / Standard") \
  X(ValueRandom,"Random") \
  X(ValueCelebrateBest,"New best") X(ValueCelebrateMilestones,"Milestones") X(ValueCelebrateBoth,"Both") \
  X(ValueSoundLow,"Low") X(ValueSoundHigh,"High") \
  X(ValueMountTop,"Top") X(ValueMountSidecar,"Sidecar") \
  X(ValueWipeAll,"Erase device data") X(ValueFirmware,"Firmware %.*s") /* release length, runtime */ \
  /* Settings list and editor */ \
  X(SettingsNextMounting,"B Next: mounting") X(SettingsNextAppearance,"B Next: appearance") \
  X(EditHoldCancels,"Hold B cancels") X(EditNextOption,"B next option ->") \
  X(EditNewSession1,"Next pull starts") X(EditNewSession2,"a new session") \
  X(EditBest1,"Each new personal") X(EditBest2,"best celebrates") \
  X(EditOneMilestone,"1 milestone, %s") /* RPM */ X(EditSteps,"%u steps %s-%s") /* steps, low RPM, high RPM */ \
  X(EditAndBests,"and new bests") X(EditTableOnline,"Edit table online") \
  X(EditMinRpm1,"Slower pulls and") X(EditMinRpm2,"rewinds not saved") \
  X(EditMount1,"Align level + replay") X(EditMount2,"to your mount") \
  X(EditFailed,"Could not apply") \
  /* New session */ \
  X(SessionKeep,"Keep past launches") X(SessionStart,"Start") \
  X(Restarting,"Saving, restarting") \
  /* Bluetooth */ \
  X(BluetoothOnExperimental,"On, experimental") X(BluetoothAppearsAs,"Appears as") X(BluetoothRadioOff,"Radio off") \
  X(BluetoothPhoneConnected,"Phone connected") X(BluetoothNoPhone,"No phone connected") \
  X(BluetoothPairedOne,"%d phone paired") X(BluetoothPairedOther,"%d phones paired") /* count */ \
  X(BluetoothBeybladeX,"Beyblade X") X(BluetoothForget,"Forget phones") \
  X(PassBluetoothOff,"Bluetooth is off") X(PassTournament,"Tournament: no RPM") X(PassAppConnected,"App connected") \
  X(PassWaiting,"Waiting for app") X(PassButton,"PASS") X(PassPressed,"pressed") X(PassHoldA,"hold A") \
  X(PassRegister1,"To register a pass,") X(PassRegister2,"hold A when asked") \
  X(PassToSync,"To sync: %u") X(PassLastSync,"Last sync: %u") /* launches */ X(PassFooter,"Hold A Button  B Back") \
  X(ForgetAgain,"Pair again afterwards") X(ForgetConfirm,"Forget") \
  X(EnableExperimental,"Experimental") X(EnableTested,"Still being tested.") X(EnableBattery1,"Uses more battery") \
  X(EnableBattery2,"while it is on.") X(EnableTurnOn,"Turn on") \
  X(PairTitle,"Pair phone?") X(PairCheck,"Check it on the phone") X(PairSame,"Same number on phone?") \
  X(PairReject,"B reject") X(PairConfirmReject,"A confirm, B reject") \
  /* Device and About */ \
  X(DeviceBattery,"Battery") X(DeviceNoReading,"Reading unavailable") X(DeviceChargeUnknown,"Charge status unknown") \
  X(DeviceCharging,"Charging") X(DeviceNotCharging,"Not charging") X(DeviceFlash,"Flash") \
  X(DeviceTotal,"Total %.2f MiB") X(DeviceTotalUnknown,"Total unavailable") \
  X(DeviceApp,"App %.2f/%.2f MiB") /* app, slot */ X(DeviceAppUnknown,"App unavailable") \
  X(DeviceAppFree,"App free %.2f MiB") X(DeviceAppFreeUnknown,"App free unavailable") X(DeviceAppSlot,"App = firmware slot") \
  X(AboutFirmware,"Firmware") X(AboutDeviceId,"Device ID") X(AboutPulls,"Pulls saved") \
  /* Wipe all */ \
  X(WipeErased,"Data erased") X(WipeRelease,"Release buttons") X(WipeToRestart,"to restart") X(WipeRestartAfter,"Restart after release") \
  X(WipeIncomplete,"Wipe incomplete") X(WipeReview,"Review and retry") X(WipeToFinish,"to finish wiping") \
  X(WipeFooterReview,"A Review  B Restart") X(WipeStep,"Step %u of 3") /* step */ \
  X(WipeErase,"Erase this device?") X(WipeLaunches,"All launches") X(WipeBests,"Personal bests") \
  X(WipeTilts,"Tilt recordings") X(WipeSettings,"All settings") X(WipeFooterContinue,"A Continue  B Cancel") \
  X(WipeUndone,"This can't be undone.") X(WipeBackUp,"Back up first") X(WipeEverything,"Erase everything?") \
  X(WipeFooterConfirm,"A Confirm  B Cancel") X(WipeHold,"Hold A for 3 sec") X(WipeReleaseStop,"Release to stop") \
  X(WipeBCancels,"B cancels") X(WipeFooterHold,"Hold A  B Cancel") X(WipeRetry,"Wipe needs retry") X(WipeIdle,"Try again when idle") \
  /* String wear check */ \
  X(WearTopMount,"Top mount") X(WearSidecarMount,"Sidecar mount") X(WearSidecarInstall,"Sidecar install?") X(WearTopInstall,"Top install?") \
  X(WearHealth,"AVG RETRACT %u%%") /* percent */ X(WearRetraction,"AVG RETRACTION") X(WearMedian,"RPM  (med %ld)") /* RPM */ \
  X(WearLevel,"Level") X(WearDown,"Down") X(WearUp,"Up") \
  X(WearStage,"%s: %ld RPM") X(WearStageWide,"%s: %ld") /* stage, RPM */ X(WearFooterDone,"A Again  Hold B Back") \
  X(WearRetryLevel,"Retry: hold 0-12 deg") X(WearRetryVertical,"Retry: hold 90 deg") X(WearRetryFlip,"Retry: flip 180 deg") \
  X(WearRetryImu,"Retry: wait for IMU") X(WearRetryLonger,"Retry: hold longer") X(WearRetryMissed,"Retry: not measured") \
  X(WearLast,"Last: %ld RPM") /* RPM */ X(WearIntro,"Pull, hold, let go") X(WearDegrees,"%.1f deg") X(WearNoDegrees,"--.- deg") \
  X(WearSettingsMount,"Settings > Mounting") X(WearFooter,"A Reset  Hold B Back")

enum class Text:uint16_t {
#define LAUNCHLAB_UI_TEXT_KEY(key,english) key,
  LAUNCHLAB_UI_TEXT(LAUNCHLAB_UI_TEXT_KEY)
#undef LAUNCHLAB_UI_TEXT_KEY
  COUNT
};
static constexpr unsigned COUNT=unsigned(Text::COUNT);
using Table=std::array<const char *,COUNT>;

static constexpr Table ENGLISH={
#define LAUNCHLAB_UI_TEXT_ENGLISH(key,english) english,
  LAUNCHLAB_UI_TEXT(LAUNCHLAB_UI_TEXT_ENGLISH)
#undef LAUNCHLAB_UI_TEXT_ENGLISH
};

// Languages by stable id (a saved setting will keep these numbers).
enum class Lang:uint8_t {English};
static constexpr unsigned LANGS=1;
static constexpr const Table *TABLES[LANGS]={&ENGLISH};

// The language the screen is drawn in; English until a setting chooses another.
inline Lang &current(){static Lang lang=Lang::English;return lang;}
inline const char *t(Text key,Lang lang) {
  const unsigned k=unsigned(key),l=unsigned(lang);
  const char *text=k<COUNT && l<LANGS?(*TABLES[l])[k]:nullptr;
  return text?text:k<COUNT?ENGLISH[k]:"";
}
inline const char *t(Text key){return t(key,current());}
}
