#pragma once
#include <Arduino.h>
#define HTTP_GET 0
#define HTTP_POST 1
struct WebServer {
 int code=0;String body;std::map<std::string,String> args;
 WebServer(int){}
 template<class F> void on(const char*,int,F){}
 template<class F> void onNotFound(F){}
 void sendHeader(const char*,const char*){}
 void send(int c,const char*,const String &b){code=c;body=b;}
 void send_P(int c,const char*,const char *b){code=c;body=b;}
 bool hasArg(const char *k){return args.count(k);}
 String arg(const char *k){return args[k];}
 void begin(){}
 void handleClient(){}
};
