#include <Arduino.h>
#include <esp_system.h>
#include <esp32_c5_touch_lcd_1_69.h>
#include <LVGL_8.4.0/lvgl/lvgl.h>
#include "BoardConfig.h"
#include "CatCore.h"

#if !ARDUINO_USB_CDC_ON_BOOT
#error "Enable Tools > USB CDC On Boot; UART pads are reserved for the S3 link."
#endif

// Official Waveshare BSP manages ST7789V2 initialization, screen offset and LVGL task.
static const bsp_display_lvgl_partial_cfg_t displayConfig={
  .use_psram=false, .double_buffer=false, .buffer_height=40
};
HardwareSerial link(1);
cat::LineReader rx;
lv_obj_t *eyes[2]{}, *pupils[2]{}, *mouth=nullptr, *caption=nullptr;
bool displayReady=false, remote=false;
String mood="neutral";
uint32_t lastRemote=0, lastCycle=0, lastPaint=0, nextBlink=0, blinkStart=0;
bool blinking=false;
uint8_t cycleIndex=0;
const char *moods[5]={"neutral","happy","curious","sleepy","angry"};
char consoleLine[64]{};size_t consoleUsed=0;bool consoleOverflow=false;
uint32_t seq=1, pendingAction=0, actionSentAt=0;bool awaitingAction=false;

void sendFrame(uint32_t id,const char *type,const char *value) {
  char out[cat::MAX_LINE];if(cat::encode(out,sizeof(out),id,type,value))link.print(out);
}
lv_obj_t *makeShape(lv_obj_t *parent,int w,int h,uint32_t color) {
  lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
  lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
  lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,12,0);
  lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
void createFace() {
  if(!bsp_display_lock(1000))return;
  lv_obj_t *screen=lv_scr_act();lv_obj_set_style_bg_color(screen,lv_color_black(),0);
  for(int i=0;i<2;++i) {
    eyes[i]=makeShape(screen,66,70,0x80E9F2);lv_obj_set_pos(eyes[i],38+i*98,90);
    pupils[i]=makeShape(eyes[i],16,48,0x061C27);lv_obj_center(pupils[i]);
  }
  mouth=lv_label_create(screen);lv_label_set_text(mouth,"w");lv_obj_set_style_text_color(mouth,lv_color_hex(0xF3BEA3),0);
  lv_obj_align(mouth,LV_ALIGN_CENTER,0,42);
  caption=lv_label_create(screen);lv_obj_set_style_text_color(caption,lv_color_hex(0x8CA1AC),0);
  lv_obj_align(caption,LV_ALIGN_BOTTOM_MID,0,-25);
  displayReady=true;bsp_display_unlock();
}
void drawFace() {
  if(!displayReady||!bsp_display_lock(20))return;
  int h=70;uint32_t color=0x80E9F2;
  if(mood=="happy"){h=35;color=0xFFCB79;}
  if(mood=="sleepy"){h=18;color=0x859BCB;}
  if(mood=="angry"){h=42;color=0xF17979;}
  if(mood=="curious"){h=82;color=0x91ECB6;}
  for(int i=0;i<2;++i) {
    int eh=blinking?6:(mood=="curious"&&i==1?52:h);
    lv_obj_set_size(eyes[i],66,eh);lv_obj_set_pos(eyes[i],38+i*98,125-eh/2);
    lv_obj_set_style_bg_color(eyes[i],lv_color_hex(color),0);
    if(eh<20)lv_obj_add_flag(pupils[i],LV_OBJ_FLAG_HIDDEN);
    else {lv_obj_clear_flag(pupils[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_height(pupils[i],eh-16);lv_obj_center(pupils[i]);}
  }
  lv_label_set_text(mouth,mood=="angry"?"_":"w");
  String text=mood+(remote?" / LINK":" / DEMO");lv_label_set_text(caption,text.c_str());
  lv_obj_align(caption,LV_ALIGN_BOTTOM_MID,0,-25);bsp_display_unlock();
}
const char *actionForText(const String &text) {
  if(text=="点头"||text=="nod")return "nod";
  if(text=="摇头"||text=="shake")return "shake";
  if(text=="招手"||text=="wave")return "wave";
  if(text=="动耳朵"||text=="ears")return "ears";
  if(text=="停止"||text=="stop")return "stop";
  if(text=="回中位"||text=="center")return "center";
  return nullptr;
}
void handleText(const String &text) {
  const char *name=actionForText(text);
  if(!name){Serial.println("Unknown text; allowed: nod/shake/wave/ears/stop/center. No cloud AI in this draft.");return;}
  bool stop=!strcmp(name,"stop");
  if(awaitingAction&&!stop){Serial.println("Previous action awaits ACK; do not resend.");return;}
  pendingAction=seq++;sendFrame(pendingAction,"ACTION",name);awaitingAction=true;actionSentAt=millis();
}
void consoleTick() {
  int budget=64;
  while(budget--&&Serial.available()) {
    char c=Serial.read();if(c=='\r')continue;
    if(c=='\n') {
      if(!consoleOverflow){consoleLine[consoleUsed]=0;String s=consoleLine;s.trim();if(s.length())handleText(s);}
      else Serial.println("Command too long");
      consoleUsed=0;consoleOverflow=false;
    } else if(!consoleOverflow) {
      if(consoleUsed>=sizeof(consoleLine)-1)consoleOverflow=true;else consoleLine[consoleUsed++]=c;
    }
  }
}
#if ENABLE_AUDIO_LOOPBACK
void audioTask(void *) {
  if(!bsp_audio_init()){Serial.println("Audio init failed");vTaskDelete(nullptr);return;}
  bsp_audio_set_speaker_volume(20);bsp_audio_set_mic_gain(12);
  uint8_t buffer[1024];
  for(;;) {
    size_t n=0,written=0;
    if(bsp_audio_read(buffer,sizeof(buffer),&n)&&n) {
      if(!bsp_audio_write(buffer,n,&written)||written!=n)Serial.println("Audio write incomplete");
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
#endif
void setup() {
  Serial.begin(115200);seq=esp_random();link.begin(LINK_BAUD,SERIAL_8N1,LINK_RX,LINK_TX);
  bsp_display_start_partial(&displayConfig);bsp_display_brightness_set(65);createFace();
  nextBlink=millis()+3000;
#if ENABLE_AUDIO_LOOPBACK
  xTaskCreate(audioTask,"audio-test",4096,nullptr,1,nullptr);
#endif
  Serial.println("DesktopCat C5 draft; USB text commands enabled. No speech recognition or cloud dialogue.");
}
void loop() {
  uint32_t now=millis();cat::Packet p;int budget=256;
  while(budget--&&link.available())if(rx.feed(link.read(),p)) {
    if(!strcmp(p.type,"MOOD")) {
      if(cat::validMood(p.value)){mood=p.value;remote=true;lastRemote=now;sendFrame(p.seq,"ACK","OK");}
      else sendFrame(p.seq,"ACK","INVALID");
    } else if(!strcmp(p.type,"ACK")&&awaitingAction&&p.seq==pendingAction) {
      Serial.printf("Action accepted/status: %s (not a completion signal)\n",p.value);awaitingAction=false;
    }
  }
  if(awaitingAction&&cat::elapsed(now,actionSentAt,1500)) {
    Serial.println("Action ACK timeout: execution unknown; no automatic replay.");awaitingAction=false;
  }
  if(remote&&cat::elapsed(now,lastRemote,LINK_TIMEOUT_MS)){remote=false;lastCycle=now;}
  if(!remote&&cat::elapsed(now,lastCycle,7000)){cycleIndex=(cycleIndex+1)%5;mood=moods[cycleIndex];lastCycle=now;}
  if(!blinking&&(int32_t)(now-nextBlink)>=0){blinking=true;blinkStart=now;}
  if(blinking&&cat::elapsed(now,blinkStart,140)){blinking=false;nextBlink=now+2500+esp_random()%2000;}
  if(cat::elapsed(now,lastPaint,50)){drawFace();lastPaint=now;}
  consoleTick();delay(2);
}
