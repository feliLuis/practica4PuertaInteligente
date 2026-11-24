#include "TestSupport.h"
#include "ReliableChannel.h"
#include "ControlLinkManager.h"
#include "SensorLinkManager.h"
#include "config.h"
#include <vector>
#include <cstring>

struct FakeLink:LocalLink{
 bool beginOk=true,isReady=true,sendOk=true;std::vector<Protocol::Message> sent;std::function<void(const Protocol::Message&)> receiver;
 bool begin()override{return beginOk;}bool send(const Protocol::Message&m)override{if(sendOk)sent.push_back(m);return sendOk;}
 void onReceive(std::function<void(const Protocol::Message&)>c)override{receiver=c;}bool ready()const override{return isReady;}
 void inject(const Protocol::Message&m){if(receiver)receiver(m);}
};
Protocol::Message message(Protocol::Type type,uint32_t session,uint32_t sequence,Protocol::DoorState door=Protocol::DoorState::UNKNOWN){Protocol::Message m{};m.version=Protocol::VERSION;m.type=type;m.sessionId=session;m.sequence=sequence;m.doorState=door;return m;}
Protocol::Message response(uint32_t session,uint32_t sequence,uint32_t targetSession,uint32_t targetSequence,Protocol::DoorState door){auto m=message(Protocol::Type::STATE_RESPONSE,session,sequence,door);m.correlationSession=targetSession;m.correlationSequence=targetSequence;return m;}

struct ControlHarness{
 FakeLink link;ReliableChannel channel{link,100};LockController controller{true};ControlLinkManager manager{controller,channel};int movements=0;
 ControlHarness(){controller.onMovement([this](Action){++movements;return true;});CHECK(manager.begin());}
 void sensor(Protocol::Message m){link.inject(m);}void close(uint32_t seq=1){sensor(message(Protocol::Type::REED_STATE,200,seq,Protocol::DoorState::CLOSED));}
 void unlock(){CHECK(controller.processAction(Action::UNLOCK,Source::DIAGNOSTIC).result==EventResult::EXECUTED);}
 uint32_t request(){CHECK(manager.requestLock(Source::DIAGNOSTIC));return link.sent.back().sequence;}
};

int main(){Suite s;
 s.testCase("peer MAC validation rejects unset multicast and unauthorized sources",[]{const uint8_t zero[6]{};const uint8_t multicast[6]={1,2,3,4,5,6};const uint8_t peer[6]={4,0xB2,0x47,0x84,0x13,0x8C};const uint8_t other[6]={4,1,2,3,4,5};CHECK(!Protocol::validUnicastMac(zero));CHECK(!Protocol::validUnicastMac(multicast));CHECK(Protocol::validUnicastMac(peer));CHECK(Protocol::authorizedSource(peer,peer));CHECK(!Protocol::authorizedSource(peer,other));});
 s.testCase("transport initialization failure leaves startup without movement",[]{FakeLink l;l.beginOk=false;ReliableChannel c(l,1);CHECK(!c.begin());LockController controller(true);int moves=0;controller.onMovement([&](Action){++moves;return true;});CHECK(moves==0&&controller.lockState()==LockState::UNKNOWN);});
 s.testCase("reliable message is acknowledged by exact correlation",[]{FakeLink l;ReliableChannel c(l,10);CHECK(c.begin());uint32_t q=c.send(Protocol::Type::STATE_REQUEST);CHECK(q&&l.sent.size()==1);auto ack=message(Protocol::Type::ACK,20,1);ack.correlationSession=10;ack.correlationSequence=q;l.inject(ack);CHECK(c.stats().acknowledgements==1);});
 s.testCase("lost ACK retries three attempts then exhausts",[]{FakeLink l;ReliableChannel c(l,10);c.begin();c.send(Protocol::Type::STATE_REQUEST);Fake::now=200;c.update();Fake::now=400;c.update();Fake::now=600;c.update();CHECK(l.sent.size()==3&&c.stats().retries==2&&c.stats().exhausted==1);});
 s.testCase("pending queue overflow is explicit and bounded",[]{FakeLink l;ReliableChannel c(l,10);c.begin();for(int i=0;i<4;++i)CHECK(c.send(Protocol::Type::STATE_REQUEST)!=0);CHECK(c.send(Protocol::Type::STATE_REQUEST)==0);CHECK(c.stats().queueOverflows==1&&l.sent.size()==4);});
 s.testCase("retry timer handles millis wraparound",[]{Fake::now=UINT32_MAX-100;FakeLink l;ReliableChannel c(l,10);c.begin();c.send(Protocol::Type::STATE_REQUEST);Fake::now+=199;c.update();CHECK(c.stats().retries==0);++Fake::now;c.update();CHECK(c.stats().retries==1);});
 s.testCase("duplicates and out of order messages are acknowledged but delivered once",[]{FakeLink l;ReliableChannel c(l,10);int delivered=0;c.onMessage([&](const Protocol::Message&){++delivered;});c.begin();auto m=message(Protocol::Type::HEARTBEAT,20,2,Protocol::DoorState::CLOSED);l.inject(m);l.inject(m);l.inject(message(Protocol::Type::HEARTBEAT,20,1,Protocol::DoorState::OPEN));CHECK(delivered==1&&c.stats().duplicates==1&&c.stats().stale==1&&l.sent.size()==3);});
 s.testCase("new peer session is detected and delayed old session is rejected",[]{FakeLink l;ReliableChannel c(l,10);int sessions=0,delivered=0;c.onSession([&](uint32_t,uint32_t){++sessions;});c.onMessage([&](const Protocol::Message&){++delivered;});c.begin();l.inject(message(Protocol::Type::HEARTBEAT,20,1));l.inject(message(Protocol::Type::HEARTBEAT,21,1));l.inject(message(Protocol::Type::RFID_PRESENTED,20,2));CHECK(sessions==2&&delivered==2&&c.stats().stale==1);});
 s.testCase("unknown heartbeat does not make the Reed reading valid",[]{ControlHarness h;h.sensor(message(Protocol::Type::HEARTBEAT,200,1));CHECK(!h.controller.sensorIsFresh()&&h.controller.doorState()==Protocol::DoorState::UNKNOWN);});
 s.testCase("valid heartbeat updates sensor and 3000 ms timeout invalidates it",[]{ControlHarness h;h.sensor(message(Protocol::Type::HEARTBEAT,200,1,Protocol::DoorState::CLOSED));CHECK(h.controller.sensorIsFresh());Fake::now=2999;h.manager.update();CHECK(h.controller.sensorIsFresh());Fake::now=3000;h.manager.update();CHECK(!h.controller.sensorIsFresh());});
 s.testCase("link status expires and recovers in the same peer session",[]{FakeLink l;ReliableChannel c(l,10);c.begin();l.inject(message(Protocol::Type::HEARTBEAT,20,1));CHECK(c.online());Fake::now=3000;CHECK(!c.online());l.inject(message(Protocol::Type::HEARTBEAT,20,2));CHECK(c.online());});
 s.testCase("new sensor session invalidates the prior Reed reading",[]{ControlHarness h;h.close();CHECK(h.controller.sensorIsFresh());h.sensor(message(Protocol::Type::HEARTBEAT,201,1));CHECK(!h.controller.sensorIsFresh()&&h.controller.doorState()==Protocol::DoorState::UNKNOWN);});
 s.testCase("RFID retry does not repeat authorization or movement",[]{ControlHarness h;h.close();auto r=message(Protocol::Type::RFID_PRESENTED,200,2,Protocol::DoorState::CLOSED);r.uidLen=4;memcpy(r.uid,Config::CARD_UID,4);h.sensor(r);h.sensor(r);CHECK(h.movements==1&&h.controller.lockState()==LockState::UNLOCKED);});
 s.testCase("both credentials are rejected through the link in blocked mode",[]{for(const uint8_t* uid:{Config::CARD_UID,Config::KEYFOB_UID}){ControlHarness h;h.controller.setMode(Mode::BLOCKED,Source::DIAGNOSTIC);auto r=message(Protocol::Type::RFID_PRESENTED,200,1);r.uidLen=4;memcpy(r.uid,uid,4);h.sensor(r);CHECK(h.movements==0&&h.controller.lockState()==LockState::UNKNOWN);}});
 s.testCase("correlated closed response permits one lock movement",[]{ControlHarness h;h.close();h.unlock();uint32_t q=h.request();h.sensor(response(200,2,100,q,Protocol::DoorState::CLOSED));CHECK(h.movements==2&&h.controller.lockState()==LockState::LOCKED&&!h.manager.confirmationPending());});
 s.testCase("auto-lock expiry requests and requires a fresh correlated response",[]{ControlHarness h;h.close();h.unlock();for(uint32_t second=1;second<=10;++second){Fake::now=second*1000;h.sensor(message(Protocol::Type::HEARTBEAT,200,second+1,Protocol::DoorState::CLOSED));h.manager.update();}CHECK(h.manager.confirmationPending()&&h.movements==1);uint32_t q=h.link.sent.back().sequence;h.sensor(response(200,12,100,q,Protocol::DoorState::CLOSED));CHECK(h.movements==2&&h.controller.lockState()==LockState::LOCKED);});
 s.testCase("missing confirmation times out without lock movement",[]{ControlHarness h;h.close();h.unlock();h.request();Fake::now=751;h.manager.update();CHECK(h.movements==1&&!h.manager.confirmationPending()&&h.controller.lockState()==LockState::UNLOCKED);});
 s.testCase("door opening while lock confirmation is pending cancels it",[]{ControlHarness h;h.close();h.unlock();uint32_t q=h.request();h.sensor(message(Protocol::Type::REED_STATE,200,2,Protocol::DoorState::OPEN));h.sensor(response(200,3,100,q,Protocol::DoorState::CLOSED));CHECK(h.movements==1&&!h.manager.confirmationPending());});
 s.testCase("old or uncorrelated response cannot authorize locking",[]{ControlHarness h;h.close();h.unlock();uint32_t q=h.request();h.sensor(response(200,2,100,q+1,Protocol::DoorState::CLOSED));CHECK(h.movements==1&&h.manager.confirmationPending());Fake::now=751;h.manager.update();CHECK(h.movements==1);});
 s.testCase("sensor node answers state request with current state and correlation",[]{FakeLink l;ReliableChannel c(l,300);SensorLinkManager sensor(c);CHECK(sensor.begin());sensor.setDoorState(Protocol::DoorState::CLOSED);l.inject(message(Protocol::Type::STATE_REQUEST,100,7));CHECK(l.sent.size()==2);const auto&r=l.sent.back();CHECK(r.type==Protocol::Type::STATE_RESPONSE&&r.doorState==Protocol::DoorState::CLOSED&&r.correlationSession==100&&r.correlationSequence==7);});
 s.testCase("sensor sends unknown without allowing it to mean closed",[]{FakeLink l;ReliableChannel c(l,300);SensorLinkManager sensor(c);sensor.begin();CHECK(sensor.sendHeartbeat(Protocol::DoorState::UNKNOWN));CHECK(l.sent.back().doorState==Protocol::DoorState::UNKNOWN);});
 s.testCase("control reboot session prevents an ACK from the former boot matching",[]{FakeLink l;ReliableChannel oldChannel(l,100);oldChannel.begin();uint32_t oldSequence=oldChannel.send(Protocol::Type::STATE_REQUEST);ReliableChannel newChannel(l,101);newChannel.begin();newChannel.send(Protocol::Type::STATE_REQUEST);auto ack=message(Protocol::Type::ACK,200,1);ack.correlationSession=100;ack.correlationSequence=oldSequence;l.inject(ack);CHECK(newChannel.stats().acknowledgements==0&&newChannel.stats().stale==1);});
 return s.result();}
