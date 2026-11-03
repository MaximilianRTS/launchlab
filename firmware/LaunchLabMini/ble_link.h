#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_heap_caps.h>
#include <esp_mac.h>
#include <Preferences.h>
#include <esp_random.h>
#include <host/ble_hs.h>
#include <host/ble_store.h>
#include <host/util/util.h>
#include <memory>
#include <services/gap/ble_svc_gap.h>
#include "ble_security.h"
#include "name_setting.h"

// Bluetooth link to the phone app; docs/BLE_PROTOCOL.md is the contract.
// Callbacks run on the NimBLE host task and only store what arrived: commands
// run in the loop, which owns all settings and history. Nothing is sent while
// a pull is being measured, apart from what `busy` lets through afterwards.
namespace BleLink {
static constexpr uint32_t START_AFTER_MS=1500,ADVERTISE_RETRY_MS=2000,FRAME_GAP_MS=15,BANNER_MS=1500;
static constexpr size_t REPLY_BYTES=4096,COMMAND_BYTES=170,MAX_FRAME_BYTES=180,FRAME_HEAD=3;
static constexpr uint8_t FLAG_LAST=1,FLAG_ERROR=2;
static constexpr char SERVICE[]="4c4c0001-6265-7977-6174-63686c61626c";
static constexpr char EVENTS[]="4c4c0002-6265-7977-6174-63686c61626c";
static constexpr char COMMAND[]="4c4c0003-6265-7977-6174-63686c61626c";
static constexpr char REPLY[]="4c4c0004-6265-7977-6174-63686c61626c";
enum class State:uint8_t {Off,Ready,Connected};

// Fills `out`; returns its length, or minus a protocol status for an error frame.
using Handler=int32_t(*)(uint8_t op,const uint8_t *args,size_t length,uint8_t *out,size_t capacity);

static BLEServer *server=nullptr;
static BLECharacteristic *events=nullptr,*reply=nullptr;
static Handler handler=nullptr;
static uint8_t *replyBuffer=nullptr;
static volatile bool linkUp=false,started=false,initFailed=false,commandWaiting=false;
static volatile uint32_t connects=0,disconnects=0,commandsDropped=0;
static uint8_t commandBytes[1+COMMAND_BYTES];
static volatile size_t commandLength=0;
static size_t replyLength=0,replySent=0;
static uint8_t replyOp=0,replySeq=0,replyFlags=0;
static bool replying=false,advertising=false;
static uint32_t lastAdvertiseTry=0,lastFrame=0,notifies=0,startedAt=0;
static size_t heapBefore=0,heapAfter=0;
static State shown=State::Off;
static bool bannerShown=false,bannerConnected=false;
static uint32_t bannerUntil=0;
// Live tilt, owned by the loop. A link lost between ticks is seen by its count.
static bool liveTilt=false;
static uint32_t disconnectsSeen=0;
// Security state: written by the host task, read by the loop, under the mux.
static BlePairing::Link securityLink;
static portMUX_TYPE securityMux=portMUX_INITIALIZER_UNLOCKED;
static volatile bool promptBusy=false;
static uint32_t securityAsked=0;
static bool keyWasShown=false;
// The stack's bond store is RAM only here (no CONFIG_BT_NIMBLE_NVS_PERSIST): keep
// its code, and copy its security records to Preferences through the settings
// save, inside a checkpoint, never in a pull. These notice a change on the host task.
using BondImage=BleBonds::Image<ble_store_value_sec>;
static ble_store_write_fn *storeWrite=nullptr;
static ble_store_delete_fn *storeDelete=nullptr;
static volatile bool bondsChanged=false,bondsRestoring=false;
static bool bondsRestored=false;
static Preferences *bondPrefs=nullptr;
static constexpr const char *BONDS_KEY="ble_bonds";
static bool isBondRecord(int type){return type==BLE_STORE_OBJ_TYPE_OUR_SEC || type==BLE_STORE_OBJ_TYPE_PEER_SEC;}
static int bondStoreWrite(int type,const ble_store_value *value) {
  const int rc=storeWrite(type,value);
  if(!rc && isBondRecord(type) && !bondsRestoring)bondsChanged=true;
  return rc;
}
static int bondStoreDelete(int type,const ble_store_key *key) {
  const int rc=storeDelete(type,key);
  if(!rc && isBondRecord(type) && !bondsRestoring)bondsChanged=true;
  return rc;
}
// The chosen name (empty: the default) and what it advertises as.
static char chosenName[BleName::MAX+1]="";
static char advertisedName[BleName::FULL]="";
static volatile bool nameChanged=false;

// The library answers a numeric comparison inside its own event handler, so it
// could not wait for a button. Its handler is private: an explicit instantiation
// may name it. Each link's events come here first, the rest pass to the library.
using GapHandler=int(*)(ble_gap_event *,void *);
GapHandler libraryGapHandler();
template<GapHandler H> struct ExposeGapHandler {friend GapHandler libraryGapHandler(){return H;}};
template struct ExposeGapHandler<&BLEServer::handleGATTServerEvent>;
static int gapEvents(ble_gap_event *event,void *arg) {
  if(event->type!=BLE_GAP_EVENT_PASSKEY_ACTION)return libraryGapHandler()(event,arg);
  // Asked only of a new phone; a bonded one restores encryption. Answered from the loop.
  const bool compare=event->passkey.params.action==BLE_SM_IOACT_NUMCMP;
  const uint32_t code=event->passkey.params.numcmp,now=millis();
  int bonds=0;ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC,&bonds);
  portENTER_CRITICAL(&securityMux);
  if(compare)securityLink.comparison(now,promptBusy,code,bonds>0);else securityLink.refuse();
  portEXIT_CRITICAL(&securityMux);
  return 0;
}
class ServerEvents:public BLEServerCallbacks {
  void onConnect(BLEServer *)override{
    linkUp=true;++connects;
    portENTER_CRITICAL(&securityMux);securityLink.connected(millis());portEXIT_CRITICAL(&securityMux);
  }
  void onConnect(BLEServer *owner,ble_gap_conn_desc *desc)override{ble_gap_set_event_cb(desc->conn_handle,gapEvents,owner);}
  void onDisconnect(BLEServer *)override{
    linkUp=false;commandWaiting=false;++disconnects;
    const uint32_t now=millis();
    portENTER_CRITICAL(&securityMux);securityLink.disconnected(now);portEXIT_CRITICAL(&securityMux);
  }
  void onAuthenticationComplete(ble_gap_conn_desc *desc)override{
    const bool encrypted=desc->sec_state.encrypted,authenticated=desc->sec_state.authenticated;
    const uint32_t now=millis();
    // A peer that could not compare (Just Works) keeps no bond here.
    if(encrypted && !authenticated)ble_store_util_delete_peer(&desc->peer_id_addr);
    portENTER_CRITICAL(&securityMux);securityLink.encryption(now,encrypted,authenticated);portEXIT_CRITICAL(&securityMux);
  }
};
class CommandEvents:public BLECharacteristicCallbacks {
  // The library calls both onWrite overloads for one write, and this one's
  // default calls the other: overriding the plain one would run commands twice.
  void onWrite(BLECharacteristic *c,ble_gap_conn_desc *)override{
    const String value=c->getValue();
    const size_t length=value.length();
    // The characteristic's permissions already require the secured link.
    if(!length || length>sizeof(commandBytes) || commandWaiting){++commandsDropped;return;}
    memcpy(commandBytes,value.c_str(),length);commandLength=length;commandWaiting=true;
  }
};

static void initTask(void *) {
  heapBefore=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  replyBuffer=static_cast<uint8_t *>(heap_caps_malloc(REPLY_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!replyBuffer){initFailed=true;vTaskDelete(nullptr);return;}
  uint8_t mac[6]={};esp_read_mac(mac,ESP_MAC_BT);
  BleName::full(advertisedName,sizeof(advertisedName),chosenName,mac);
  BLEDevice::init(advertisedName);
  BLEDevice::setMTU(MAX_FRAME_BYTES+3);
  // LE Secure Connections, numeric comparison only: the stick shows the number and A
  // confirms. MITM and Secure Connections only leave no Just Works or legacy way in.
  // Directly: on NimBLE, BLESecurity's setters use statics the core never defines.
  ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_YESNO;
  ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;ble_hs_cfg.sm_sc_only=1;
  ble_hs_cfg.sm_our_key_dist=ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
  storeWrite=ble_hs_cfg.store_write_cb;storeDelete=ble_hs_cfg.store_delete_cb;
  ble_hs_cfg.store_write_cb=bondStoreWrite;ble_hs_cfg.store_delete_cb=bondStoreDelete;
  server=BLEDevice::createServer();
  server->setCallbacks(new ServerEvents());
  auto *service=server->createService(SERVICE);
  // The stack takes these flags as they are. Subscribing has no permission of its
  // own, so these reads carry it, and the library then notifies encrypted links only;
  // the loop also sends nothing until the link is secured.
  const uint32_t protectedRead=BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_READ_ENC|BLECharacteristic::PROPERTY_READ_AUTHEN;
  events=service->createCharacteristic(EVENTS,BLECharacteristic::PROPERTY_NOTIFY|protectedRead);
  reply=service->createCharacteristic(REPLY,BLECharacteristic::PROPERTY_NOTIFY|protectedRead);
  auto *command=service->createCharacteristic(COMMAND,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_ENC|BLECharacteristic::PROPERTY_WRITE_AUTHEN);
  command->setCallbacks(new CommandEvents());
  service->start();
  auto *ad=BLEDevice::getAdvertising();
  ad->addServiceUUID(SERVICE);
  ad->setScanResponse(true);
  heapAfter=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  started=true;
  vTaskDelete(nullptr);
}

inline bool connected(){return started && linkUp;}
inline bool secured() {
  portENTER_CRITICAL(&securityMux);const bool out=securityLink.secured();portEXIT_CRITICAL(&securityMux);
  return out && connected();
}
// The 6-digit number while a pairing waits for A or B.
inline bool comparison(uint32_t &number,bool &automatic) {
  portENTER_CRITICAL(&securityMux);const bool out=securityLink.comparing();number=securityLink.number;automatic=securityLink.automatic;portEXIT_CRITICAL(&securityMux);
  return out && connected();
}
// From the loop, never the host task: the stack waits for this. A refusal ends the link.
inline void answerComparison(bool accept) {
  const uint32_t now=millis();
  portENTER_CRITICAL(&securityMux);const bool asked=securityLink.answer(now,accept);portEXIT_CRITICAL(&securityMux);
  if(!asked)return;
  ble_sm_io io{};io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=accept;
  ble_sm_inject_io(server->getConnId(),&io);
}
// A reply's last frame has had time to leave: a restart may follow it.
inline bool replyFlushed(uint32_t now){return !replying && now-lastFrame>=300;}
// No prompt during a pull, an editor, Wipe or tournament: refused, not queued.
inline void setPromptBusy(bool busy){promptBusy=busy;}
inline int bondCount() {
  int bonds=0;
  if(started)ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC,&bonds);
  else if(bondPrefs && bondPrefs->isKey(BONDS_KEY)) {
    // Not started yet: the saved record's own count, without decoding its keys.
    uint8_t head[BleBonds::HEADER];
    if(bondPrefs->getBytesLength(BONDS_KEY)>=sizeof(head)) {
      auto whole=std::unique_ptr<uint8_t[]>(new(std::nothrow) uint8_t[bondPrefs->getBytesLength(BONDS_KEY)]);
      if(whole && bondPrefs->getBytes(BONDS_KEY,whole.get(),bondPrefs->getBytesLength(BONDS_KEY))>=sizeof(head)){std::memcpy(head,whole.get(),sizeof(head));bonds=head[3]&15;}
    }
  }
  return bonds;
}
inline bool takeBondsChanged(){const bool out=bondsChanged;bondsChanged=false;return out;}
// Forget phones: ends the link and empties the store; the caller queues the save.
inline void forget() {
  if(!started)return;
  if(linkUp)server->disconnect(server->getConnId());
  ble_store_clear();
}
// The record, from the loop inside a checkpoint. Empty removes it.
static int bondCollect(int type,ble_store_value *value,void *image) {
  static_cast<BondImage *>(image)->add(type==BLE_STORE_OBJ_TYPE_OUR_SEC,value->sec);return 0;
}
inline bool saveBonds(Preferences &prefs) {
  if(!started)return !prefs.isKey(BONDS_KEY) || prefs.remove(BONDS_KEY);
  auto image=std::unique_ptr<BondImage>(new(std::nothrow) BondImage);
  if(!image)return false;
  ble_store_iterate(BLE_STORE_OBJ_TYPE_OUR_SEC,bondCollect,image.get());
  ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC,bondCollect,image.get());
  if(!image->ours && !image->peers)return !prefs.isKey(BONDS_KEY) || prefs.remove(BONDS_KEY);
  const size_t capacity=image->bytes();
  auto bytes=std::unique_ptr<uint8_t[]>(new(std::nothrow) uint8_t[capacity]);
  if(!bytes)return false;
  const size_t n=image->encode(bytes.get(),capacity);
  return n && prefs.putBytes(BONDS_KEY,bytes.get(),n)==n;
}
// Before the first advertising, so no phone can arrive first. Reading NVS is safe in a pull.
static void restoreBonds() {
  if(!bondPrefs || !bondPrefs->isKey(BONDS_KEY))return;
  const size_t n=bondPrefs->getBytesLength(BONDS_KEY);
  auto bytes=std::unique_ptr<uint8_t[]>(new(std::nothrow) uint8_t[n?n:1]);
  auto image=std::unique_ptr<BondImage>(new(std::nothrow) BondImage);
  if(!bytes || !image || bondPrefs->getBytes(BONDS_KEY,bytes.get(),n)!=n || !image->decode(bytes.get(),n)) {
    Serial.println("BLE_BONDS_NOTICE unreadable_record_retained");return;
  }
  bondsRestoring=true;
  for(unsigned i=0;i<image->ours;++i)ble_store_write_our_sec(&image->our[i]);
  for(unsigned i=0;i<image->peers;++i)ble_store_write_peer_sec(&image->peer[i]);
  bondsRestoring=false;
}
// Set name: saved by the caller, advertised from the next advertising.
inline void setName(const char *name,size_t length) {
  std::memcpy(chosenName,name,length);chosenName[length]=0;nameChanged=true;
}
inline bool saveName(Preferences &prefs){return NameSetting::save(prefs,chosenName);}
inline void loadName(Preferences &prefs){if(!NameSetting::load(prefs,chosenName))Serial.println("BLE_NAME_NOTICE invalid_saved_name_retained_using_default");}
inline const char *fullName() {
  if(!advertisedName[0] || nameChanged) {
    uint8_t mac[6]={};esp_read_mac(mac,ESP_MAC_BT);
    BleName::full(advertisedName,sizeof(advertisedName),chosenName,mac);
  }
  return advertisedName;
}
inline State state(bool enabled) {
  if(!enabled || !started)return State::Off;
  return linkUp?State::Connected:State::Ready;
}

// Called with each accepted pull, after it has ended.
inline void publishLaunch(uint32_t number,uint32_t session,float rpm) {
  if(!secured() || !events)return;
  uint8_t payload[13];payload[0]=1;
  memcpy(payload+1,&number,4);memcpy(payload+5,&session,4);memcpy(payload+9,&rpm,4);
  events->setValue(payload,sizeof(payload));events->notify();++notifies;
}

// Live tilt: asked for by the connected phone, and forgotten with its link.
inline void setLiveTilt(bool on){liveTilt=on && secured();}
inline bool liveTiltOn(){return liveTilt && secured();}
inline void publishEvent(uint8_t *payload,size_t length) {
  if(!secured() || !events)return;
  events->setValue(payload,length);events->notify();++notifies;
}

static size_t framePayload() {
  uint16_t mtu=server?server->getPeerMTU(server->getConnId()):23;
  if(mtu<23)mtu=23;
  return std::min<size_t>(mtu-3,MAX_FRAME_BYTES)-FRAME_HEAD;
}

static void sendFrame() {
  const size_t payload=std::min(framePayload(),replyLength-replySent);
  const bool last=replySent+payload>=replyLength;
  uint8_t frame[MAX_FRAME_BYTES];
  frame[0]=replyOp;frame[1]=replySeq++;frame[2]=replyFlags|(last?FLAG_LAST:0);
  memcpy(frame+FRAME_HEAD,replyBuffer+replySent,payload);
  reply->setValue(frame,FRAME_HEAD+payload);reply->notify();++notifies;
  replySent+=payload;lastFrame=millis();
  if(last)replying=false;
}

// Returns true when what the screen icon shows has changed.
inline bool tick(uint32_t now,bool enabled,bool busy) {
  static bool launched=false;
  if(enabled && !launched && !busy && now>=START_AFTER_MS) {
    launched=true;startedAt=now;
    // BLE init recurses deeply: give it its own stack, not the 8 KB loop task's.
    if(xTaskCreatePinnedToCore(initTask,"bleinit",8192,nullptr,1,nullptr,1)!=pdPASS)initFailed=true;
  }
  if(started) {
    if(disconnects!=disconnectsSeen || !enabled){disconnectsSeen=disconnects;liveTilt=false;}
    if(!bondsRestored){bondsRestored=true;restoreBonds();}
    if(nameChanged && !linkUp) {
      // Applies from the next advertising; the library keeps its own copy of the name.
      nameChanged=false;
      uint8_t mac[6]={};esp_read_mac(mac,ESP_MAC_BT);
      BleName::full(advertisedName,sizeof(advertisedName),chosenName,mac);
      ble_svc_gap_device_name_set(advertisedName);
      if(ble_gap_adv_active())BLEDevice::getAdvertising()->stop();
      BLEDevice::getAdvertising()->setName(advertisedName);
    }
    if(linkUp) {
      // Ask at once on a new link: a bonded phone restores, a new one compares.
      if(securityAsked!=connects){securityAsked=connects;ble_gap_security_initiate(server->getConnId());}
      portENTER_CRITICAL(&securityMux);const bool end=securityLink.takeDrop(now);portEXIT_CRITICAL(&securityMux);
      if(end)server->disconnect(server->getConnId());
      portENTER_CRITICAL(&securityMux);const bool confirm=securityLink.takeAutoAccept(now);portEXIT_CRITICAL(&securityMux);
      if(confirm) {
        ble_sm_io io{};io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=1;
        ble_sm_inject_io(server->getConnId(),&io);
      }
    }
    if(!enabled) {
      if(linkUp)server->disconnect(server->getConnId());
      if(ble_gap_adv_active())BLEDevice::getAdvertising()->stop();
      advertising=false;commandWaiting=false;replying=false;
    } else if(!linkUp && !ble_gap_adv_active() && now-lastAdvertiseTry>=ADVERTISE_RETRY_MS) {
      lastAdvertiseTry=now;advertising=BLEDevice::getAdvertising()->start();
    }
    if(!linkUp)replying=false;
    // One phone at a time; the stack would otherwise accept a second link.
    else if(ble_gap_adv_active())BLEDevice::getAdvertising()->stop();
    if(enabled && linkUp && !busy && secured()) {
      if(replying) {
        if(now-lastFrame>=FRAME_GAP_MS)sendFrame();
      } else if(commandWaiting && handler) {
        const uint8_t op=commandBytes[0];
        const int32_t result=handler(op,commandBytes+1,commandLength-1,replyBuffer,REPLY_BYTES);
        commandWaiting=false;
        replyOp=op;replySeq=0;replySent=0;
        if(result<0){replyBuffer[0]=uint8_t(-result);replyLength=1;replyFlags=FLAG_ERROR;}
        else {replyLength=size_t(result);replyFlags=0;}
        replying=true;sendFrame();
      }
    }
  }
  const State now_shown=state(enabled);
  bool changed=now_shown!=shown;
  // The number's screen comes and goes with the pairing.
  uint32_t code=0;bool automatic=false;const bool keyShown=comparison(code,automatic);
  if(keyShown!=keyWasShown){keyWasShown=keyShown;changed=true;}
  // Say so when a phone arrives or leaves; turning Bluetooth on or off is the user's own act.
  if(changed && (now_shown==State::Connected || (shown==State::Connected && now_shown==State::Ready))) {
    bannerConnected=now_shown==State::Connected;bannerUntil=now+BANNER_MS;bannerShown=true;
  }
  if(bannerShown && int32_t(now-bannerUntil)>=0){bannerShown=false;changed=true;}
  shown=now_shown;
  return changed;
}

template<class Canvas>
inline void drawIcon(Canvas &g,uint16_t readyColor,uint16_t connectedColor,uint16_t background,uint16_t text) {
  if(shown==State::Off)return;
  if(bannerShown) {
    g.fillRoundRect(6,2,g.width()-12,22,5,background);
    g.drawRoundRect(6,2,g.width()-12,22,5,bannerConnected?connectedColor:readyColor);
    g.setTextDatum(top_center);g.setTextColor(text,background);
    g.drawString(bannerConnected?"Phone connected":"Phone left",g.width()/2,5,2);
    return;
  }
  const uint16_t c=shown==State::Connected?connectedColor:readyColor;
  const int x=g.width()-13,y=2;
  // A rune, two pixels wide so it reads from arm's length, over a plate so it stays clear on any page.
  g.fillRect(x-1,y-1,12,17,background);
  for(int d=0;d<2;++d) {
    g.drawLine(x+4+d,y,x+4+d,y+14,c);
    g.drawLine(x+4+d,y,x+8+d,y+4,c);g.drawLine(x+8+d,y+4,x+d,y+10,c);
    g.drawLine(x+4+d,y+14,x+8+d,y+10,c);g.drawLine(x+8+d,y+10,x+d,y+4,c);
  }
}

inline void status() {
  Serial.printf("BLE_STATUS started=%d init_failed=%d state=%u adv_active=%d connects=%lu disconnects=%lu dropped=%lu notifies=%lu mtu=%u heap_before=%u heap_after=%u\n",
    started,initFailed,unsigned(shown),started?ble_gap_adv_active():-1,(unsigned long)connects,(unsigned long)disconnects,(unsigned long)commandsDropped,
    (unsigned long)notifies,unsigned(connected()?server->getPeerMTU(server->getConnId()):0),unsigned(heapBefore),unsigned(heapAfter));
}
}
