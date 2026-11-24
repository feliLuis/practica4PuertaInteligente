#include "ReliableChannel.h"
#include <Arduino.h>
#include <string.h>
ReliableChannel::ReliableChannel(LocalLink& l,uint32_t s):link_(l),sessionId_(s),nextSequence_(0),peerSession_(0),highestSequence_(0),lastReceived_(0),retiredSessions_{},retiredCount_(0),retiredIndex_(0),stats_{}{memset(pending_,0,sizeof(pending_));}
bool ReliableChannel::begin(){if(!sessionId_)return false;link_.onReceive([this](const Protocol::Message&m){receive(m);});return link_.begin();}
bool ReliableChannel::transmit(Pending&p){++p.attempts;p.sentAt=millis();if(!link_.send(p.message)){++stats_.sendFailures;return false;}return true;}
uint32_t ReliableChannel::send(Protocol::Type t,Protocol::DoorState d,const uint8_t* uid,uint8_t len,uint32_t cs,uint32_t cq,bool reliable){
 Protocol::Message m{};m.version=Protocol::VERSION;m.type=t;m.sessionId=sessionId_;m.sequence=++nextSequence_;if(!m.sequence)m.sequence=++nextSequence_;m.doorState=d;m.uidLen=len;m.correlationSession=cs;m.correlationSequence=cq;m.uptimeMs=millis();if(len&&uid)memcpy(m.uid,uid,len);if(Protocol::validate(m)!=Protocol::ValidationError::NONE)return 0;
 if(!reliable||t==Protocol::Type::ACK){if(!link_.send(m))++stats_.sendFailures;return m.sequence;}
 for(uint8_t i=0;i<MAX_PENDING;++i)if(!pending_[i].used){pending_[i].message=m;pending_[i].used=true;pending_[i].attempts=0;transmit(pending_[i]);return m.sequence;}
 ++stats_.queueOverflows;return 0;
}
void ReliableChannel::sendAck(const Protocol::Message&m){send(Protocol::Type::ACK,Protocol::DoorState::UNKNOWN,nullptr,0,m.sessionId,m.sequence,false);}
bool ReliableChannel::retired(uint32_t s)const{for(uint8_t i=0;i<retiredCount_;++i)if(retiredSessions_[i]==s)return true;return false;}
void ReliableChannel::retire(uint32_t s){if(!s)return;retiredSessions_[retiredIndex_]=s;retiredIndex_=uint8_t((retiredIndex_+1)%4);if(retiredCount_<4)++retiredCount_;}
void ReliableChannel::clearPending(){for(uint8_t i=0;i<MAX_PENDING;++i)pending_[i].used=false;}
void ReliableChannel::receive(const Protocol::Message&m){
 ++stats_.received;if(Protocol::validate(m)!=Protocol::ValidationError::NONE){++stats_.stale;return;}
 lastReceived_=millis();
 if(m.type==Protocol::Type::ACK){if(m.correlationSession!=sessionId_){++stats_.stale;return;}for(uint8_t i=0;i<MAX_PENDING;++i)if(pending_[i].used&&pending_[i].message.sequence==m.correlationSequence){pending_[i].used=false;++stats_.acknowledgements;return;}++stats_.stale;return;}
 sendAck(m);if(retired(m.sessionId)){++stats_.stale;return;}
 if(peerSession_!=m.sessionId){uint32_t old=peerSession_;retire(peerSession_);peerSession_=m.sessionId;highestSequence_=0;clearPending();++stats_.sessionChanges;if(sessionCallback_)sessionCallback_(old,peerSession_);}
 if(m.sequence<=highestSequence_){if(m.sequence==highestSequence_)++stats_.duplicates;else++stats_.stale;return;}highestSequence_=m.sequence;++stats_.delivered;if(messageCallback_)messageCallback_(m);
}
void ReliableChannel::update(){link_.update();uint32_t now=millis();for(uint8_t i=0;i<MAX_PENDING;++i)if(pending_[i].used&&uint32_t(now-pending_[i].sentAt)>=ACK_TIMEOUT_MS){if(pending_[i].attempts>=MAX_ATTEMPTS){pending_[i].used=false;++stats_.exhausted;}else{++stats_.retries;transmit(pending_[i]);}}}
void ReliableChannel::onMessage(std::function<void(const Protocol::Message&)>c){messageCallback_=c;}void ReliableChannel::onSession(std::function<void(uint32_t,uint32_t)>c){sessionCallback_=c;}
uint32_t ReliableChannel::localSession()const{return sessionId_;}uint32_t ReliableChannel::peerSession()const{return peerSession_;}bool ReliableChannel::online()const{return link_.ready()&&peerSession_!=0&&uint32_t(millis()-lastReceived_)<LINK_TIMEOUT_MS;}const ReliableChannel::Stats& ReliableChannel::stats()const{return stats_;}
