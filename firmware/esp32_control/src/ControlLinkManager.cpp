#include "ControlLinkManager.h"
#include "config.h"
#include <Arduino.h>
ControlLinkManager::ControlLinkManager(LockController& c,ReliableChannel& ch):controller_(c),channel_(ch),pending_(false),requestSequence_(0),peerSession_(0),deadline_(0),source_(Source::EDGE),rejected_(0){}
bool ControlLinkManager::begin(){
 channel_.onMessage([this](const Protocol::Message&m){receive(m);});
 channel_.onSession([this](uint32_t a,uint32_t b){sessionChanged(a,b);});
 controller_.onLockRequest([this](Source s){requestLock(s);});
 if(!channel_.begin())return false;
 // Announces this boot session immediately so the sensor can retire pre-reboot traffic.
 channel_.send(Protocol::Type::STATE_REQUEST);
 return true;
}
void ControlLinkManager::sessionChanged(uint32_t,uint32_t current){peerSession_=current;cancelConfirmation();controller_.invalidateSensor(Source::EDGE);}
void ControlLinkManager::cancelConfirmation(){pending_=false;requestSequence_=0;}
bool ControlLinkManager::requestLock(Source source){
 if(controller_.lockState()==LockState::LOCKED)return true;
 if(!channel_.online()){++rejected_;return false;}
 cancelConfirmation();uint32_t sequence=channel_.send(Protocol::Type::STATE_REQUEST);
 if(!sequence){++rejected_;return false;}pending_=true;requestSequence_=sequence;peerSession_=channel_.peerSession();deadline_=millis()+Config::STATE_CONFIRMATION_TIMEOUT_MS;source_=source;return true;
}
void ControlLinkManager::receive(const Protocol::Message&m){
 if(m.type==Protocol::Type::REED_STATE){controller_.updateDoor(m.doorState,Source::EDGE);if(pending_&&m.doorState==Protocol::DoorState::OPEN)cancelConfirmation();return;}
 if(m.type==Protocol::Type::HEARTBEAT){controller_.receiveHeartbeat(m.doorState,Source::EDGE);if(pending_&&m.doorState==Protocol::DoorState::OPEN)cancelConfirmation();return;}
 if(m.type==Protocol::Type::RFID_PRESENTED){if(m.doorState==Protocol::DoorState::OPEN||m.doorState==Protocol::DoorState::CLOSED)controller_.updateDoor(m.doorState,Source::EDGE);controller_.processUid(m.uid,m.uidLen,Source::RFID);return;}
 if(m.type!=Protocol::Type::STATE_RESPONSE)return;
 const bool timely=static_cast<int32_t>(deadline_-millis())>=0;
 const bool correlated=pending_&&m.sessionId==peerSession_&&m.correlationSession==channel_.localSession()&&m.correlationSequence==requestSequence_;
 if(!correlated||!timely||m.doorState!=Protocol::DoorState::CLOSED){++rejected_;if(correlated)cancelConfirmation();return;}
 cancelConfirmation();controller_.updateDoor(Protocol::DoorState::CLOSED,Source::EDGE);controller_.processAction(Action::LOCK,source_);
}
void ControlLinkManager::update(){channel_.update();controller_.update();if(pending_&&static_cast<int32_t>(millis()-deadline_)>=0){cancelConfirmation();++rejected_;}}
bool ControlLinkManager::confirmationPending()const{return pending_;}uint32_t ControlLinkManager::rejectedConfirmations()const{return rejected_;}
