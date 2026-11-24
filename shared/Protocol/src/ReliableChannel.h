#pragma once
#include "LocalLink.h"
#include <functional>
class ReliableChannel {
public:
 static constexpr uint8_t MAX_PENDING=4, MAX_ATTEMPTS=3;
 static constexpr uint32_t ACK_TIMEOUT_MS=200;
 static constexpr uint32_t LINK_TIMEOUT_MS=3000;
 struct Stats{uint32_t received,delivered,duplicates,stale,sessionChanges,acknowledgements,retries,exhausted,queueOverflows,sendFailures;};
 ReliableChannel(LocalLink& link,uint32_t sessionId);
 bool begin(); void update();
 uint32_t send(Protocol::Type type,Protocol::DoorState door=Protocol::DoorState::UNKNOWN,const uint8_t* uid=nullptr,uint8_t uidLen=0,uint32_t correlationSession=0,uint32_t correlationSequence=0,bool reliable=true);
 void onMessage(std::function<void(const Protocol::Message&)> callback);
 void onSession(std::function<void(uint32_t,uint32_t)> callback);
 uint32_t localSession()const;uint32_t peerSession()const;bool online()const;const Stats& stats()const;
private:
 struct Pending{Protocol::Message message;uint32_t sentAt;uint8_t attempts;bool used;};
 void receive(const Protocol::Message&);bool transmit(Pending&);void sendAck(const Protocol::Message&);
 bool retired(uint32_t session)const;void retire(uint32_t session);void clearPending();
 LocalLink& link_;uint32_t sessionId_,nextSequence_,peerSession_,highestSequence_,lastReceived_;uint32_t retiredSessions_[4];uint8_t retiredCount_,retiredIndex_;Pending pending_[MAX_PENDING];Stats stats_;
 std::function<void(const Protocol::Message&)> messageCallback_;std::function<void(uint32_t,uint32_t)> sessionCallback_;
};
