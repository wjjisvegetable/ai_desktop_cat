// Compiles real S3 source with lightweight hardware mocks. NOT an ESP32 toolchain build.
#define CAT_HOST_TEST 1
#include "../CatS3Controller/CatS3Controller.ino"
#include <cassert>
#include <iostream>
int main(){
 setup();assert(!armed);for(auto &s:sv)assert(!s.attached);
 assert(!strcmp(runAction("arm"),"NO_CHANNEL"));
 assert(!strcmp(runAction("unknown"),"UNKNOWN"));
 assert(!strcmp(runAction("nod"),"DISARMED"));
 sv[1].enabled=true;assert(!strcmp(runAction("arm"),"UNCALIBRATED"));
 sv[1].calibrated=true;assert(!strcmp(runAction("arm"),"OK"));assert(armed&&sv[1].attached);
 assert(!strcmp(runAction("wave"),"NO_CHANNEL"));
 fakeMillis=0;assert(!strcmp(runAction("nod"),"OK"));
 assert(!strcmp(runAction("shake"),"BUSY"));
 for(int i=0;i<50;++i){int before=sv[1].pulse;fakeMillis+=20;motionTick(fakeMillis);assert(abs(sv[1].pulse-before)<=4);assert(sv[1].pulse>=1400&&sv[1].pulse<=1600);}
 int held=sv[1].pulse;assert(!strcmp(runAction("stop"),"OK"));fakeMillis+=20;motionTick(fakeMillis);assert(sv[1].pulse==held&&sv[1].attached);
 runAction("release");assert(!armed);for(auto&s:sv)assert(!s.attached);
 for(auto &s:sv){s.enabled=true;s.calibrated=true;}
 runAction("arm");runAction("dance");
 for(int i=0;i<300;++i){fakeMillis+=20;motionTick(fakeMillis);}
 assert(action=="idle");for(auto&s:sv)assert(s.pulse==s.mid);
 status();assert(server.body.find("\"armed\":true")!=std::string::npos);
 size_t pos=0,count=0;while((pos=server.body.find("\"attached\":",pos))!=std::string::npos){++count;++pos;}assert(count==8);
 saveCalibration();assert(server.code==409);
 runAction("release");server.args={{"id","0"},{"min","1000"},{"center","1500"},{"max","2000"},{"enabled","1"},{"reverse","0"},{"confirmed","1"}};
 saveCalibration();assert(server.code==200&&sv[0].lo==1000&&sv[0].hi==2000);
 server.args["min"]="9999";saveCalibration();assert(server.code==400&&sv[0].lo==1000);
 server.args={{"id","0"},{"pulse","1700"}};testServo();assert(server.code==200&&testChannel==0);
 for(int i=0;i<260;++i){fakeMillis+=20;motionTick(fakeMillis);}assert(testChannel==-1&&!sv[0].attached);
 std::cout<<"PASS: S3 default disarm, missing calibration, action channel checks, busy rejection, slew, stop hold, release, centering, 8-channel JSON, calibration rejection, test timeout\n";
}
