#pragma once
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "BleProtocol.h"

struct BleMessage { char data[catble::MAX_COMMAND+1]; };
static QueueHandle_t bleQueue=nullptr;
static NimBLECharacteristic *bleStatus=nullptr;

class CatCommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic,NimBLEConnInfo &) override {
    std::string value=characteristic->getValue();
    if(value.empty()||value.size()>catble::MAX_COMMAND||!bleQueue)return;
    BleMessage message{};
    memcpy(message.data,value.data(),value.size());
    // Callback is a task context, not an interrupt. Servo work stays in loop().
    xQueueSend(bleQueue,&message,0);
  }
};
class CatServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer *,NimBLEConnInfo &,int) override {
    NimBLEDevice::startAdvertising();
  }
};
inline void bleBegin() {
  bleQueue=xQueueCreate(4,sizeof(BleMessage));
  if(!bleQueue){Serial.println("BLE queue allocation failed");return;}
  NimBLEDevice::init("DesktopCat-S3");
  NimBLEServer *server=NimBLEDevice::createServer();
  server->setCallbacks(new CatServerCallbacks());
  NimBLEService *service=server->createService(catble::SERVICE_UUID);
  auto *command=service->createCharacteristic(catble::COMMAND_UUID,NIMBLE_PROPERTY::WRITE);
  command->setCallbacks(new CatCommandCallbacks());
  bleStatus=service->createCharacteristic(catble::STATUS_UUID,NIMBLE_PROPERTY::READ|NIMBLE_PROPERTY::NOTIFY);
  bleStatus->setValue("READY");
  service->start();
  NimBLEAdvertising *advertising=NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(catble::SERVICE_UUID);
  advertising->start();
  Serial.println("BLE: DesktopCat-S3 advertising");
}
inline void bleReply(const String &value) {
  if(!bleStatus)return;
  bleStatus->setValue(value.c_str());
  bleStatus->notify();
}
