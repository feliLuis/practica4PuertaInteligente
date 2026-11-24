#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "config.h"
#include "DoorSensor.h"
#include "RfidReader.h"
#include "EspNowLink.h"
#include "EspNowSettings.h"
#include "ReliableChannel.h"
#include "SensorLinkManager.h"

DoorSensor doorSensor;
RfidReader rfidReader(Config::RC522_SS,Config::RC522_RST);
EspNowLink::Config linkConfig=espNowSettings();
EspNowLink espNowLink(linkConfig);
ReliableChannel reliableChannel(espNowLink,esp_random()|1U);
SensorLinkManager sensorLink(reliableChannel);
uint32_t lastHeartbeat=0;

void printLinkStatus(){
 const auto&t=espNowLink.stats();const auto&r=reliableChannel.stats();
 Serial.printf("localStaMac=%s peerMac=%02X:%02X:%02X:%02X:%02X:%02X channel=%u initialized=%s link=%s encryption=%s rx=%lu tx=%lu invalid=%lu unauthorized=%lu overflow=%lu retries=%lu exhausted=%lu\n",
  WiFi.macAddress().c_str(),linkConfig.peerMac[0],linkConfig.peerMac[1],linkConfig.peerMac[2],linkConfig.peerMac[3],linkConfig.peerMac[4],linkConfig.peerMac[5],linkConfig.channel,
  espNowLink.ready()?"yes":"no",reliableChannel.online()?"online":"offline",linkConfig.encryptionEnabled?"enabled":"LAB_UNENCRYPTED",
  static_cast<unsigned long>(t.received),static_cast<unsigned long>(t.sent),static_cast<unsigned long>(t.invalidFrames),static_cast<unsigned long>(t.unauthorized),static_cast<unsigned long>(t.overflows),static_cast<unsigned long>(r.retries),static_cast<unsigned long>(r.exhausted));
}

void setup(){
 Serial.begin(Config::SERIAL_BAUD);
 if(!sensorLink.begin())Serial.println("[BOOT] ESP-NOW disabled: configure a valid peer MAC and channel.");
 rfidReader.begin();doorSensor.begin();lastHeartbeat=millis();
 Serial.println("[BOOT] Reed is unknown until stable for 100 ms. No UID is logged in normal mode.");
 Serial.println(Config::SHOW_FULL_UID?"[DIAG] Full UID output enabled on USB only.":"[NORMAL] UID hidden on USB.");
 printLinkStatus();
}

void loop(){
 sensorLink.update();
 if(doorSensor.update()){sensorLink.setDoorState(doorSensor.state());sensorLink.sendDoor(doorSensor.state());}
 rfidReader.update();uint8_t uid[10];uint8_t length=0;
 if(rfidReader.read(uid,sizeof(uid),length))sensorLink.sendRfid(uid,length,doorSensor.state());
 uint32_t now=millis();if(uint32_t(now-lastHeartbeat)>=Config::HEARTBEAT_MS){lastHeartbeat=now;sensorLink.sendHeartbeat(doorSensor.state());}
 if(Serial.available()){char line[16];size_t n=Serial.readBytesUntil('\n',line,sizeof(line)-1);line[n]='\0';if(strcmp(line,"link status")==0)printLinkStatus();}
}
