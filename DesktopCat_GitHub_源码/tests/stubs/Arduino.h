#pragma once
#include <string>
#include <deque>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define PROGMEM
#define LOW 0
#define HIGH 1
#define OUTPUT 1
#define INPUT_PULLUP 2
#define SERIAL_8N1 0
#define PI 3.14159265358979323846
#define constrain(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))
inline uint32_t fakeMillis=0;
inline uint32_t millis(){return fakeMillis;}
inline void delay(unsigned n){fakeMillis+=n;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline int digitalRead(int){return 1;}
inline bool ledcAttach(int,int,int){return true;}
inline bool ledcWrite(int,uint32_t){return true;}
inline bool ledcDetach(int){return true;}
class String: public std::string {
public:
 using std::string::string;
 String()=default;
 String(const std::string&s):std::string(s){}
 String(int n):std::string(std::to_string(n)){}
 bool isEmpty()const{return empty();}
 long toInt()const{return strtol(c_str(),nullptr,10);}
 void trim(){auto a=find_first_not_of(" \r\n\t");auto b=find_last_not_of(" \r\n\t");*this=a==npos?String():substr(a,b-a+1);}
};
class HardwareSerial {
public:
 std::deque<char> input;std::string output;
 HardwareSerial(int=0){}
 void inject(const std::string &s){for(char c:s)input.push_back(c);}
 void begin(unsigned long,int=0,int=-1,int=-1){}
 int available(){return input.size();}
 int read(){if(input.empty())return -1;char c=input.front();input.pop_front();return c;}
 void print(const char *s){output+=s;}
 void println(const char*){}
 void printf(const char*,...){}
};
inline HardwareSerial Serial;
