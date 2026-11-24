#pragma once
#include "LockController.h"
#include "ReliableChannel.h"
class ControlLinkManager {
public:
 ControlLinkManager(LockController& controller,ReliableChannel& channel);
 bool begin();void update();bool requestLock(Source source);
 bool confirmationPending()const;uint32_t rejectedConfirmations()const;
private:
 void receive(const Protocol::Message&);void sessionChanged(uint32_t,uint32_t);void cancelConfirmation();
 LockController& controller_;ReliableChannel& channel_;bool pending_;uint32_t requestSequence_,peerSession_,deadline_;Source source_;uint32_t rejected_;
};
