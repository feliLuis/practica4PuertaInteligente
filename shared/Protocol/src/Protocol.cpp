#include "Protocol.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
namespace Protocol {
static void put32(uint8_t* o,uint32_t v){o[0]=v;o[1]=v>>8;o[2]=v>>16;o[3]=v>>24;}
static uint32_t get32(const uint8_t* i){return uint32_t(i[0])|(uint32_t(i[1])<<8)|(uint32_t(i[2])<<16)|(uint32_t(i[3])<<24);}
const char* name(DoorState s){switch(s){case DoorState::OPEN:return "open";case DoorState::CLOSED:return "closed";default:return "unknown";}}
const char* name(Type t){switch(t){case Type::REED_STATE:return "reed_state";case Type::RFID_PRESENTED:return "rfid_presented";case Type::HEARTBEAT:return "heartbeat";case Type::ACK:return "ack";case Type::STATE_REQUEST:return "state_request";case Type::STATE_RESPONSE:return "state_response";default:return "unknown";}}
const char* name(ValidationError e){switch(e){case ValidationError::NONE:return "none";case ValidationError::SIZE:return "size";case ValidationError::VERSION:return "version";case ValidationError::TYPE:return "type";case ValidationError::SESSION:return "session";case ValidationError::SEQUENCE:return "sequence";case ValidationError::DOOR_STATE:return "door_state";case ValidationError::UID_LENGTH:return "uid_length";default:return "content";}}
bool validUidLength(uint8_t n){return n==4||n==7||n==10;}
bool validUnicastMac(const uint8_t* m){if(!m||(m[0]&1))return false;uint8_t any=0;for(uint8_t i=0;i<6;++i)any|=m[i];return any!=0;}
bool authorizedSource(const uint8_t* expected,const uint8_t* received){return validUnicastMac(expected)&&received&&memcmp(expected,received,6)==0;}
ValidationError validate(const Message& m){
 if(m.version!=VERSION)return ValidationError::VERSION;
 if(m.type<Type::REED_STATE||m.type>Type::STATE_RESPONSE)return ValidationError::TYPE;
 if(!m.sessionId)return ValidationError::SESSION;if(!m.sequence)return ValidationError::SEQUENCE;
 if(m.doorState>DoorState::CLOSED)return ValidationError::DOOR_STATE;
 if(m.uidLen>MAX_UID_LENGTH)return ValidationError::UID_LENGTH;
 for(uint8_t i=m.uidLen;i<MAX_UID_LENGTH;++i)if(m.uid[i])return ValidationError::CONTENT;
 const bool corr=m.correlationSession&&m.correlationSequence;
 switch(m.type){
  case Type::RFID_PRESENTED:if(!validUidLength(m.uidLen)||m.correlationSession||m.correlationSequence)return ValidationError::CONTENT;break;
  case Type::ACK:if(m.uidLen||!corr||m.doorState!=DoorState::UNKNOWN)return ValidationError::CONTENT;break;
  case Type::STATE_RESPONSE:if(m.uidLen||!corr)return ValidationError::CONTENT;break;
  case Type::REED_STATE:if(m.doorState==DoorState::UNKNOWN||m.uidLen||m.correlationSession||m.correlationSequence)return ValidationError::CONTENT;break;
  case Type::HEARTBEAT:if(m.uidLen||m.correlationSession||m.correlationSequence)return ValidationError::CONTENT;break;
  case Type::STATE_REQUEST:if(m.uidLen||m.correlationSession||m.correlationSequence||m.doorState!=DoorState::UNKNOWN)return ValidationError::CONTENT;break;
 }
 return ValidationError::NONE;
}
bool encode(const Message& m,uint8_t* o,size_t cap){if(!o||cap<WIRE_SIZE||validate(m)!=ValidationError::NONE)return false;memset(o,0,WIRE_SIZE);o[0]=m.version;o[1]=uint8_t(m.type);put32(o+2,m.sessionId);put32(o+6,m.sequence);o[10]=uint8_t(m.doorState);o[11]=m.uidLen;memcpy(o+12,m.uid,MAX_UID_LENGTH);put32(o+22,m.correlationSession);put32(o+26,m.correlationSequence);put32(o+30,m.uptimeMs);return true;}
ValidationError decode(const uint8_t* d,size_t n,Message& m){memset(&m,0,sizeof(m));if(!d||n!=WIRE_SIZE)return ValidationError::SIZE;m.version=d[0];m.type=Type(d[1]);m.sessionId=get32(d+2);m.sequence=get32(d+6);m.doorState=DoorState(d[10]);m.uidLen=d[11];memcpy(m.uid,d+12,MAX_UID_LENGTH);m.correlationSession=get32(d+22);m.correlationSequence=get32(d+26);m.uptimeMs=get32(d+30);return validate(m);}
bool uidToHex(const uint8_t* u,uint8_t n,char* o,size_t cap){if(!u||!o||n>MAX_UID_LENGTH||cap<size_t(n*2+1))return false;for(uint8_t i=0;i<n;++i)snprintf(o+i*2,3,"%02X",u[i]);o[n*2]='\0';return true;}
static int hexValue(char c){if(c>='0'&&c<='9')return c-'0';c=char(toupper(static_cast<unsigned char>(c)));return c>='A'&&c<='F'?c-'A'+10:-1;}
bool hexToUid(const char* text,uint8_t* uid,uint8_t cap,uint8_t& len){len=0;if(!text||!uid)return false;char s[32];size_t n=0;for(const char* p=text;*p;++p){if(isspace(static_cast<unsigned char>(*p)))continue;if(n>=sizeof(s)-1)return false;s[n++]=*p;}if(!n||(n&1))return false;uint8_t count=uint8_t(n/2);if(count>cap)return false;for(uint8_t i=0;i<count;++i){int h=hexValue(s[i*2]),l=hexValue(s[i*2+1]);if(h<0||l<0)return false;uid[i]=uint8_t((h<<4)|l);}len=count;return true;}
}
