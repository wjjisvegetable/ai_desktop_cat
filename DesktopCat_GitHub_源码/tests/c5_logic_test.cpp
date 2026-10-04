#define ARDUINO_USB_CDC_ON_BOOT 1
#include "../CatC5Display/CatC5Display.ino"
#include <cassert>
#include <iostream>
int main(){
 setup();assert(displayReady&&!remote&&mood=="neutral");
 char buf[cat::MAX_LINE];cat::encode(buf,sizeof(buf),42,"MOOD","sleepy");link.inject(buf);loop();assert(remote&&mood=="sleepy");assert(link.output.find(",ACK,OK*")!=std::string::npos);
 fakeMillis+=50;loop();assert(eyes[0]->h==18&&bool(pupils[0]->flags&LV_OBJ_FLAG_HIDDEN));
 fakeMillis+=5001;loop();assert(!remote);
 fakeMillis+=7000;loop();assert(mood=="happy");
 handleText("nod");assert(awaitingAction);auto old=pendingAction;handleText("wave");assert(pendingAction==old);
 handleText("stop");assert(awaitingAction&&pendingAction!=old);
 cat::encode(buf,sizeof(buf),pendingAction,"ACK","OK");link.inject(buf);loop();assert(!awaitingAction);
 handleText("something not allowed");assert(!awaitingAction);
 handleText("点头");assert(awaitingAction);fakeMillis+=1600;loop();assert(!awaitingAction);
 for(auto *o:allocated)delete o;
 std::cout<<"PASS: C5 mood ACK, sleepy face, 5s link fallback, 7s cycle, text whitelist, pending action, stop priority, ACK timeout\n";
}
