#include "SensorLinkManager.h"
SensorLinkManager::SensorLinkManager(ReliableChannel& c):channel_(c),doorState_(Protocol::DoorState::UNKNOWN){}
bool SensorLinkManager::begin(){channel_.onMessage([this](const Protocol::Message&m){receive(m);});return channel_.begin();}
void SensorLinkManager::update(){channel_.update();}
void SensorLinkManager::setDoorState(Protocol::DoorState s){doorState_=s;}
bool SensorLinkManager::sendDoor(Protocol::DoorState s){doorState_=s;return channel_.send(Protocol::Type::REED_STATE,s)!=0;}
bool SensorLinkManager::sendRfid(const uint8_t* uid,uint8_t n,Protocol::DoorState s){doorState_=s;return channel_.send(Protocol::Type::RFID_PRESENTED,s,uid,n)!=0;}
bool SensorLinkManager::sendHeartbeat(Protocol::DoorState s){doorState_=s;return channel_.send(Protocol::Type::HEARTBEAT,s,nullptr,0,0,0,false)!=0;}
void SensorLinkManager::receive(const Protocol::Message&m){if(m.type==Protocol::Type::STATE_REQUEST)channel_.send(Protocol::Type::STATE_RESPONSE,doorState_,nullptr,0,m.sessionId,m.sequence);}
