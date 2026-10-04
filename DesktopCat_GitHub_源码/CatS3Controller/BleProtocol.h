#pragma once
#include <cstring>

namespace catble {
constexpr const char *SERVICE_UUID="42d60001-7568-4f24-9ca4-9d1a4d3e0001";
constexpr const char *COMMAND_UUID="42d60002-7568-4f24-9ca4-9d1a4d3e0001";
constexpr const char *STATUS_UUID ="42d60003-7568-4f24-9ca4-9d1a4d3e0001";
constexpr size_t MAX_COMMAND=80;
enum class Kind { Invalid, Action, Mood };
struct Command { Kind kind=Kind::Invalid; char value[20]{}; };

inline bool allowedAction(const char *s) {
  const char *names[]={"nod","shake","wave","wave_right","both","ears","center","dance","stop","release"};
  for(const char *name:names)if(!strcmp(s,name))return true;
  return false;
}
inline bool allowedMood(const char *s) {
  const char *names[]={"auto","neutral","happy","curious","sleepy","angry"};
  for(const char *name:names)if(!strcmp(s,name))return true;
  return false;
}
// Wire format: <private-token>|A:<action> or <private-token>|M:<mood>.
// Arm and calibration are deliberately excluded from the BLE surface.
inline Command parse(const char *raw,const char *token) {
  Command result;
  if(!raw||!token||!token[0])return result;
  size_t tokenLen=strlen(token), rawLen=strnlen(raw,MAX_COMMAND+1);
  if(rawLen>MAX_COMMAND||tokenLen<12||tokenLen>40||rawLen<=tokenLen+3)return result;
  unsigned diff=0;
  for(size_t i=0;i<tokenLen;++i)diff|=(unsigned char)(raw[i]^token[i]);
  if(diff||raw[tokenLen]!='|'||raw[tokenLen+2]!=':')return result;
  char mode=raw[tokenLen+1];const char *value=raw+tokenLen+3;
  if(strlen(value)>=sizeof(result.value))return result;
  if(mode=='A'&&allowedAction(value))result.kind=Kind::Action;
  else if(mode=='M'&&allowedMood(value))result.kind=Kind::Mood;
  if(result.kind!=Kind::Invalid)strcpy(result.value,value);
  return result;
}
}
