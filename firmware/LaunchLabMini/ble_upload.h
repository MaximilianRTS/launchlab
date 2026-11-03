#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Ops 24-26: a phone sends the same framed requests the website sends over USB,
// a theme or a milestone or effect set request, in pieces. The checks and the save
// are the USB code's, so nothing here judges what the bytes say.
namespace BleUpload {
enum Kind:uint8_t {Milestones=1,Theme=2};
// Set by the largest request, the v3 milestone table.
constexpr size_t MAX_BYTES=232,RESULT_CHARS=180;
class Buffer {
public:
  // Starts over, whatever came before: a link can end halfway.
  bool begin(uint8_t kind,uint16_t length) {
    reset();
    if((kind!=Milestones && kind!=Theme) || !length || length>MAX_BYTES)return false;
    kind_=kind;length_=length;return true;
  }
  // In order only, so a lost write cannot leave a hole that passes as complete.
  bool add(uint16_t offset,const uint8_t *bytes,size_t n) {
    if(!kind_ || offset!=received_ || !n || n>size_t(length_-received_))return false;
    memcpy(data_+offset,bytes,n);received_+=uint16_t(n);return true;
  }
  bool complete()const{return kind_ && received_==length_;}
  void reset(){kind_=0;length_=received_=0;}
  uint8_t kind()const{return kind_;}
  size_t length()const{return length_;}
  const uint8_t *data()const{return data_;}
private:
  uint8_t data_[MAX_BYTES]={},kind_=0;
  uint16_t length_=0,received_=0;
};
// Op 26's reply: status 0 ok, 2 busy, 1 anything else, then the request's own
// result line for the app to show, without its line end.
inline size_t encodeResult(const char *line,uint8_t *out) {
  const bool ok=strstr(line,"_OK");
  out[0]=ok?0:strstr(line,"reason=busy")?2:1;
  size_t n=strlen(line);
  while(n && (line[n-1]=='\n' || line[n-1]=='\r'))--n;
  if(n>RESULT_CHARS-1)n=RESULT_CHARS-1;
  out[1]=uint8_t(n);memcpy(out+2,line,n);
  return n+2;
}
}
