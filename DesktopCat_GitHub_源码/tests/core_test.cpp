#include "../CatS3Controller/CatCore.h"
#include <assert.h>
#include <string>
#include <iostream>
int main() {
  char line[cat::MAX_LINE];cat::Packet p;
  assert(cat::encode(line,sizeof(line),42,"MOOD","happy"));
  cat::LineReader reader;bool ok=false;
  for(char *s=line;*s;++s)ok=reader.feed(*s,p)||ok;
  assert(ok && p.seq==42 && !strcmp(p.type,"MOOD") && !strcmp(p.value,"happy"));
  std::string encoded=line;encoded.pop_back();
  for(size_t i=0;i<encoded.size();++i) {
    std::string damaged=encoded;damaged[i]^=1;assert(!cat::parse(damaged.c_str(),p));
  }
  assert(!cat::encode(line,10,0,"ACTION","nod"));
  assert(!cat::encode(line,sizeof(line),0,"MOOD","happy\ninject"));
  assert(!cat::encode(line,sizeof(line),0,"TYPE_TOO_LONG","ok"));
  assert(!cat::parse("CAT1,4294967296,MOOD,happy*00",p));
  // Buffer overflow discards through newline, then resumes with the next valid frame.
  for(int i=0;i<200;++i)assert(!reader.feed('X',p));
  assert(!reader.feed('\n',p));
  assert(cat::encode(line,sizeof(line),UINT32_MAX,"ACTION","stop"));
  ok=false;for(char *s=line;*s;++s)ok=reader.feed(*s,p)||ok;
  assert(ok && p.seq==UINT32_MAX && !strcmp(p.value,"stop"));
  assert(!cat::validLimits(499,1500,2500));assert(!cat::validLimits(1500,1500,1600));
  assert(!cat::validLimits(1000,2000,1800));assert(cat::validLimits(1000,1450,2000));
  for(int k=-200;k<=200;++k) {
    int a=cat::pulseFromOffset(1000,1450,2000,k/100.0f,false);
    assert(a>=1000&&a<=2000);
    assert(a==cat::pulseFromOffset(1000,1450,2000,-k/100.0f,true));
  }
  assert(cat::pulseFromOffset(1000,1450,2000,0,false)==1450);
  assert(cat::pulseFromOffset(1000,1450,2000,2,false)==2000);
  assert(cat::pulseFromOffset(1000,1450,2000,-2,false)==1000);
  assert(cat::elapsed(15,UINT32_MAX-9,25));assert(!cat::elapsed(15,UINT32_MAX-9,26));
  assert(cat::validMood("sleepy")&&!cat::validMood("auto")&&!cat::validMood("unknown"));
  // Random input and all short truncations must fail without memory errors.
  for(size_t i=0;i<encoded.size();++i)assert(!cat::parse(encoded.substr(0,i).c_str(),p));
  uint32_t seed=7;
  for(int i=0;i<10000;++i) {
    std::string s;for(int j=0;j<i%95;++j){seed=seed*1664525u+1013904223u;s+=char(33+seed%94);}
    cat::parse(s.c_str(),p);
  }
  std::cout<<"PASS: framing, corruption, overflow recovery, input bounds, pulse limits, reverse, timer rollover; 10000 parser fuzz inputs\n";
}
