#pragma once
#define WIFI_AP 1
struct WiFiClass{void mode(int){} bool softAP(const char*,const char*){return true;}};
inline WiFiClass WiFi;
