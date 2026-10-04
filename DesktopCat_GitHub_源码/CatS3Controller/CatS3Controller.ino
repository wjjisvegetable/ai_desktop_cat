#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <math.h>
#include "BoardConfig.h"
#include "CatCore.h"
#include "RemotePage.h"
#include "CalibrationPage.h"
#include "BleProtocol.h"
#if !defined(CAT_HOST_TEST)
#include "BleControl.h"
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
constexpr const char *BLE_CONTROL_TOKEN="";
#endif
#endif

// Arduino-ESP32 3.x. No servo library is required; uses the eight S3 LEDC channels.
struct ServoState {
  int lo=1400, mid=1500, hi=1600, pulse=1500;
  bool enabled=false, calibrated=false, reverse=false, attached=false;
};
ServoState sv[8];
WebServer server(80);
Preferences prefs;
HardwareSerial link(1);
cat::LineReader rx;
bool armed=false, dark=false, rawDark=false;
uint32_t rawChanged=0, motionStart=0, tickAt=0;
String action="idle", manualMood="auto", mood="neutral";
int startPulse[8];
uint32_t sequence=1, moodSeq=0, sentAt=0, lastMoodAck=0;
bool moodAcked=false, everAcked=false;
String pendingMood="";
bool actionSeen=false; uint32_t actionSeq=0; String lastResult="";
int testChannel=-1, testTarget=1500; uint32_t testStarted=0;

void sendFrame(uint32_t seq,const char *type,const char *value) {
  char out[cat::MAX_LINE]; if(cat::encode(out,sizeof(out),seq,type,value))link.print(out);
}
void writePulse(int i,int pulse) {
  sv[i].pulse=constrain(pulse,sv[i].lo,sv[i].hi);
  if(sv[i].attached)ledcWrite(SERVO_PINS[i],uint32_t(sv[i].pulse)*((1UL<<PWM_BITS)-1)/20000UL);
}
bool attachChannel(int i) {
  if(sv[i].attached)return true;
  if(!ledcAttach(SERVO_PINS[i],PWM_HZ,PWM_BITS))return false;
  sv[i].attached=true;writePulse(i,sv[i].pulse);return true;
}
void detachChannel(int i) {
  if(sv[i].attached)ledcDetach(SERVO_PINS[i]);
  sv[i].attached=false;
}
void stopMotion() {action="idle";testChannel=-1;}
void releaseAll() {stopMotion();for(int i=0;i<8;++i)detachChannel(i);armed=false;}
void rememberStart() {for(int i=0;i<8;++i)startPulse[i]=sv[i].pulse;motionStart=millis();}
bool enabled(int i) {return sv[i].enabled && sv[i].calibrated;}
const char *runAction(const String &name) {
  if(name=="stop") {stopMotion();return "OK";}
  if(name=="release") {releaseAll();return "OK";}
  if(name=="arm") {
    if(armed)return "OK";
    bool any=false;for(int i=0;i<8;++i) {
      if(sv[i].enabled && !sv[i].calibrated)return "UNCALIBRATED";
      any |= enabled(i);
    }
    if(!any)return "NO_CHANNEL";
    releaseAll();
    for(int i=0;i<8;++i)if(enabled(i) && !attachChannel(i)){releaseAll();return "PWM_ERROR";}
    armed=true;return "OK";
  }
  if(name!="nod"&&name!="shake"&&name!="wave"&&name!="wave_right"&&name!="both"&&name!="ears"&&name!="center"&&name!="dance")return "UNKNOWN";
  if(!armed)return "DISARMED";
  if(action!="idle")return "BUSY";
  if((name=="nod"&&!enabled(1))||(name=="shake"&&!enabled(0))||(name=="wave"&&!enabled(4))||
     (name=="wave_right"&&!enabled(5))||(name=="both"&&(!enabled(4)||!enabled(5)))||
     (name=="ears"&&(!enabled(2)||!enabled(3)))||
     (name=="dance"&&(!enabled(0)||!enabled(1)||!enabled(2)||!enabled(3)||!enabled(4)||!enabled(5))))return "NO_CHANNEL";
  action=name;rememberStart();return "OK";
}
void motionTick(uint32_t now) {
  if(!cat::elapsed(now,tickAt,20))return;
  tickAt=now;
  if(testChannel>=0) {
    int i=testChannel;
    if(cat::elapsed(now,testStarted,5000)){detachChannel(i);testChannel=-1;return;}
    writePulse(i,sv[i].pulse+constrain(testTarget-sv[i].pulse,-4,4));return;
  }
  if(!armed||action=="idle")return;
  float t=(now-motionStart)/1000.0f;
  const float duration=action=="center"?1.5f:3.0f;
  float ease=fminf(t/0.6f,1.0f);
  // Small amplitude, ramped into and out of a sine cycle. Limits are per-channel.
  float envelope=fminf(fminf(t/0.6f,(duration-t)/0.6f),1.0f);
  envelope=fmaxf(0.0f,envelope);
  float wave=0.65f*sinf(t*2*PI)*envelope;
  for(int i=0;i<8;++i)if(enabled(i)) {
    float x=0;
    if((action=="nod"&&i==1)||(action=="shake"&&i==0)||(action=="wave"&&i==4)||
       (action=="wave_right"&&i==5)||(action=="both"&&(i==4||i==5))||
       (action=="ears"&&(i==2||i==3))||(action=="dance"&&i<6))x=wave;
    int target=cat::pulseFromOffset(sv[i].lo,sv[i].mid,sv[i].hi,x,sv[i].reverse);
    int requested=startPulse[i]+int((target-startPulse[i])*ease);
    // A universal 4 us / 20 ms slew cap also applies when returning from a stopped pose.
    writePulse(i,sv[i].pulse+constrain(requested-sv[i].pulse,-4,4));
  }
  if(t>=duration) {
    bool centered=true;for(int i=0;i<8;++i)if(enabled(i)&&abs(sv[i].pulse-sv[i].mid)>4)centered=false;
    if(centered){for(int i=0;i<8;++i)if(enabled(i))writePulse(i,sv[i].mid);action="idle";}
  }
}
void lightTick(uint32_t now) {
  bool v=digitalRead(LIGHT_PIN)==LIGHT_DARK_LEVEL;
  if(v!=rawDark){rawDark=v;rawChanged=now;}
  if(cat::elapsed(now,rawChanged,250))dark=rawDark;
  mood=manualMood!="auto"?manualMood:(dark?"sleepy":(action!="idle"?"happy":"neutral"));
}
void linkTick(uint32_t now) {
  cat::Packet p; int budget=256;
  while(budget-- && link.available())if(rx.feed(link.read(),p)) {
    if(!strcmp(p.type,"ACK")&&p.seq==moodSeq&&!strcmp(p.value,"OK")) {
      moodAcked=true;everAcked=true;lastMoodAck=now;
    } else if(!strcmp(p.type,"ACTION")) {
      if(!actionSeen||p.seq!=actionSeq) {
        // Remote link cannot arm or alter calibration; only local App can enable power.
        String n=p.value;
        lastResult=(n=="arm")?"LOCAL_ONLY":runAction(n);
        actionSeq=p.seq;actionSeen=true;
      }
      sendFrame(p.seq,"ACK",lastResult.c_str());
    }
  }
  if(pendingMood!=mood || (moodAcked&&cat::elapsed(now,sentAt,1000))) {
    pendingMood=mood;moodSeq=sequence++;moodAcked=false;
    sendFrame(moodSeq,"MOOD",pendingMood.c_str());sentAt=now;
  } else if(!moodAcked&&cat::elapsed(now,sentAt,300)) {
    sendFrame(moodSeq,"MOOD",pendingMood.c_str());sentAt=now;
  }
}
void reply(int code,const String &text) {
  server.sendHeader("Cache-Control","no-store");
  server.send(code,"text/plain; charset=utf-8",text);
}
void status() {
  String out="{\"armed\":";out+=armed?"true":"false";
  out+=",\"dark\":";out+=dark?"true":"false";
  out+=",\"action\":\""+action+"\",\"mood\":\""+mood+"\",\"c5_connected\":";
  out+=(everAcked&&!cat::elapsed(millis(),lastMoodAck,3000))?"true":"false";
  out+=",\"servos\":[";
  for(int i=0;i<8;++i) {
    if(i)out+=",";
    out+="{\"id\":"+String(i)+",\"name\":\""+SERVO_NAMES[i]+"\",\"attached\":"+(sv[i].attached?"true":"false");
    out+=",\"enabled\":";out+=sv[i].enabled?"true":"false";
    out+=",\"calibrated\":";out+=sv[i].calibrated?"true":"false";
    out+=",\"reverse\":";out+=sv[i].reverse?"true":"false";
    out+=",\"min\":"+String(sv[i].lo)+",\"center\":"+String(sv[i].mid)+",\"max\":"+String(sv[i].hi)+",\"pulse\":"+String(sv[i].pulse)+"}";
  }
  out+="]}";server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",out);
}
bool numberArg(const char *key,int &value) {
  if(!server.hasArg(key))return false;
  String s=server.arg(key);if(s.isEmpty()||s.length()>4)return false;
  for(size_t i=0;i<s.length();++i)if(s[i]<'0'||s[i]>'9')return false;
  value=s.toInt();return true;
}
void saveCalibration() {
  int id,lo,mid,hi,en,rev,ok;
  bool attached=false;for(int i=0;i<8;++i)attached|=sv[i].attached;
  if(armed||attached){reply(409,"请先关闭 PWM 再修改校准");return;}
  if(!numberArg("id",id)||id>7||!numberArg("min",lo)||!numberArg("center",mid)||!numberArg("max",hi)||
     !numberArg("enabled",en)||en>1||!numberArg("reverse",rev)||rev>1||!numberArg("confirmed",ok)||ok>1||!cat::validLimits(lo,mid,hi)) {
    reply(400,"参数错误，需要 500 <= min < center < max <= 2500");return;
  }
  sv[id].lo=lo;sv[id].mid=mid;sv[id].hi=hi;sv[id].enabled=en;sv[id].reverse=rev;sv[id].calibrated=ok;
  sv[id].pulse=constrain(sv[id].pulse,lo,hi);
  String k="s"+String(id);
  // Explicit packed numeric fields, not ABI-dependent raw structs.
  uint16_t vals[7]={(uint16_t)lo,(uint16_t)mid,(uint16_t)hi,(uint16_t)en,(uint16_t)rev,(uint16_t)ok,1};
  if(prefs.putBytes(k.c_str(),vals,sizeof(vals))!=sizeof(vals)){reply(500,"保存失败，请重启并检查配置");return;}
  reply(200,"已保存；请先逐路试动，确认后再勾选校准完成");
}
void testServo() {
  int id,pulse;
  if(armed){reply(409,"请先关闭 PWM");return;}
  if(!numberArg("id",id)||id>7||!numberArg("pulse",pulse)||pulse<sv[id].lo||pulse>sv[id].hi){reply(400,"试动参数超出已保存范围");return;}
  if(testChannel>=0&&testChannel!=id)detachChannel(testChannel);
  testChannel=-1;
  if(!attachChannel(id)){reply(500,"PWM_ERROR");return;}
  testChannel=id;testTarget=pulse;testStarted=millis();reply(200,"单路试动中，5 秒后关闭输出");
}
void bleTick() {
#if !defined(CAT_HOST_TEST)
  if(!bleQueue)return;
  BleMessage message{};
  if(xQueueReceive(bleQueue,&message,0)!=pdTRUE)return;
  catble::Command command=catble::parse(message.data,BLE_CONTROL_TOKEN);
  if(command.kind==catble::Kind::Invalid){bleReply(BLE_CONTROL_TOKEN[0]?"ERR:AUTH_OR_COMMAND":"ERR:SET_TOKEN");return;}
  if(command.kind==catble::Kind::Action){
    const char *result=runAction(command.value);
    bleReply(String("ACTION:")+command.value+":"+result);
  } else {
    manualMood=command.value;
    bleReply(String("MOOD:")+command.value+":OK");
  }
#endif
}
void setup() {
  Serial.begin(115200);
  for(int i=0;i<8;++i){pinMode(SERVO_PINS[i],OUTPUT);digitalWrite(SERVO_PINS[i],LOW);}
  prefs.begin("desktopcat",false);
  for(int i=0;i<8;++i) {
    String k="s"+String(i);uint16_t v[7]{};
    if(prefs.getBytes(k.c_str(),v,sizeof(v))==sizeof(v)&&v[6]==1&&cat::validLimits(v[0],v[1],v[2])&&v[3]<=1&&v[4]<=1&&v[5]<=1) {
      sv[i].lo=v[0];sv[i].mid=v[1];sv[i].hi=v[2];sv[i].enabled=v[3];sv[i].reverse=v[4];sv[i].calibrated=v[5];sv[i].pulse=v[1];
    }
  }
  pinMode(LIGHT_PIN,INPUT_PULLUP);rawDark=digitalRead(LIGHT_PIN)==LIGHT_DARK_LEVEL;rawChanged=millis();
  sequence=esp_random();link.begin(LINK_BAUD,SERIAL_8N1,LINK_RX,LINK_TX);
  WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASSWORD);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",CALIBRATION_PAGE);});
  server.on("/remote",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",REMOTE_PAGE);});
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/action",HTTP_POST,[]{
    String n=server.arg("name");const char *r=runAction(n);
    reply(!strcmp(r,"OK")?200:(!strcmp(r,"UNKNOWN")?400:409),!strcmp(r,"OK")?"指令已接受，请读取状态确认执行进度":r);
  });
  server.on("/api/mood",HTTP_POST,[]{String n=server.arg("name");
    if(n!="auto"&&!cat::validMood(n.c_str())){reply(400,"未知表情");return;}
    manualMood=n;reply(200,"表情已接受；C5 是否同步请查看状态");
  });
  server.on("/api/calibration",HTTP_POST,saveCalibration);
  server.on("/api/test",HTTP_POST,testServo);
  server.onNotFound([]{reply(404,"Not found");});server.begin();
  #if !defined(CAT_HOST_TEST)
  bleBegin();
  #endif
  Serial.println("DesktopCat S3 draft: http://192.168.4.1 ; PWM OFF");
}
void loop() {
  server.handleClient();uint32_t now=millis();
  bleTick();motionTick(now);lightTick(now);linkTick(now);delay(1);
}
