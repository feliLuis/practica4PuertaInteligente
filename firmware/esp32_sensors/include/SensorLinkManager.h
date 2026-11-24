#pragma once
#include "ReliableChannel.h"
class SensorLinkManager {
public:
 explicit SensorLinkManager(ReliableChannel& channel);bool begin();void update();
 bool sendDoor(Protocol::DoorState state);bool sendRfid(const uint8_t* uid,uint8_t length,Protocol::DoorState state);bool sendHeartbeat(Protocol::DoorState state);
 void setDoorState(Protocol::DoorState state);
private:void receive(const Protocol::Message& message);ReliableChannel& channel_;Protocol::DoorState doorState_;
};
