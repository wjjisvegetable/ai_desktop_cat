#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Portable, bounded UART protocol. One complete frame per line.
namespace cat {
constexpr size_t MAX_LINE = 96;
struct Packet { uint32_t seq; char type[12]; char value[24]; };
inline bool token(const char *s) {
  if (!s || !*s) return false;
  for (; *s; ++s) if (!((*s >= 'a' && *s <= 'z') ||
      (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')) return false;
  return true;
}
inline uint8_t checksum(const char *s, size_t n) {
  uint8_t c = 0; for (size_t i = 0; i < n; ++i) c ^= (uint8_t)s[i]; return c;
}
inline bool encode(char *out, size_t cap, uint32_t seq, const char *type, const char *value) {
  if (!token(type) || !token(value) || strlen(type) >= 12 || strlen(value) >= 24) return false;
  int n = snprintf(out, cap, "CAT1,%lu,%s,%s", (unsigned long)seq, type, value);
  if (n < 0 || (size_t)n + 5 > cap) return false;
  uint8_t sum = checksum(out, n);
  snprintf(out + n, cap - n, "*%02X\n", sum); return true;
}
inline int hex(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='A'&&c<='F')return c-'A'+10;
  if(c>='a'&&c<='f')return c-'a'+10;
  return -1;
}
inline bool parse(const char *line, Packet &p) {
  size_t n = strlen(line); if (n < 13 || n >= MAX_LINE) return false;
  const char *star = strchr(line, '*');
  if (!star || strlen(star) != 3 || hex(star[1]) < 0 || hex(star[2]) < 0) return false;
  if (checksum(line, star-line) != (hex(star[1])*16+hex(star[2]))) return false;
  char buf[MAX_LINE]; memcpy(buf, line, star-line); buf[star-line]=0;
  char *fields[4]; fields[0]=buf;
  for(int i=1;i<4;++i) { char *c=strchr(fields[i-1],','); if(!c)return false; *c=0; fields[i]=c+1; }
  if(strcmp(fields[0],"CAT1") || strchr(fields[3],','))return false;
  uint64_t id=0; if(!*fields[1])return false;
  for(const char *s=fields[1];*s;++s) {
    if(*s<'0'||*s>'9')return false; id=id*10+(*s-'0'); if(id>UINT32_MAX)return false;
  }
  if(!token(fields[2]) || !token(fields[3]) || strlen(fields[2])>=sizeof(p.type) || strlen(fields[3])>=sizeof(p.value))return false;
  p.seq=(uint32_t)id; strcpy(p.type,fields[2]); strcpy(p.value,fields[3]); return true;
}
class LineReader {
  char buf[MAX_LINE]{}; size_t used=0; bool discard=false;
public:
  bool feed(char c, Packet &p) {
    if(c=='\r')return false;
    if(c=='\n') { bool ok=false; if(!discard){buf[used]=0;ok=parse(buf,p);} used=0;discard=false;return ok; }
    if(discard)return false;
    if(c<32 || c>126 || used>=MAX_LINE-1){discard=true;return false;}
    buf[used++]=c;return false;
  }
};
inline bool validMood(const char *s) {
  return !strcmp(s,"neutral")||!strcmp(s,"happy")||!strcmp(s,"curious")||!strcmp(s,"sleepy")||!strcmp(s,"angry");
}
inline bool validLimits(int lo,int mid,int hi) {
  return lo>=500 && hi<=2500 && lo<mid && mid<hi;
}
inline int pulseFromOffset(int lo,int mid,int hi,float x,bool reverse) {
  if(x < -1)x=-1; if(x>1)x=1; if(reverse)x=-x;
  int p=(int)(mid + (x>=0 ? (hi-mid)*x : (mid-lo)*x));
  return p<lo ? lo : (p>hi ? hi : p);
}
// Unsigned subtraction intentionally tolerates millis() rollover.
inline bool elapsed(uint32_t now,uint32_t then,uint32_t duration) {return uint32_t(now-then)>=duration;}
}
