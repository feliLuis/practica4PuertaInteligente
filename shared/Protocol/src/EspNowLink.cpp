#include "EspNowLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

EspNowLink* EspNowLink::instance_=nullptr;
static portMUX_TYPE queueMux=portMUX_INITIALIZER_UNLOCKED;
EspNowLink::EspNowLink(const Config& c):config_(c),stats_{},head_(0),tail_(0),count_(0),ready_(false){}
bool EspNowLink::validPeerMac(const uint8_t* m){return Protocol::validUnicastMac(m);}
bool EspNowLink::begin(){
 // STA is initialized first so diagnostics can always report the real local STA MAC.
 WiFi.mode(WIFI_STA);
 if(!config_.peerConfigured||!validPeerMac(config_.peerMac)||config_.channel<1||config_.channel>14)return false;
 if(esp_wifi_set_channel(config_.channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK)return false;
 if(esp_now_init()!=ESP_OK)return false;instance_=this;
 esp_now_register_recv_cb(receiveCallback);esp_now_register_send_cb(sendCallback);
 if(config_.encryptionEnabled&&esp_now_set_pmk(config_.pmk)!=ESP_OK)return false;
 esp_now_peer_info_t peer{};memcpy(peer.peer_addr,config_.peerMac,6);peer.channel=config_.channel;peer.encrypt=config_.encryptionEnabled;if(peer.encrypt)memcpy(peer.lmk,config_.lmk,16);
 ready_=esp_now_add_peer(&peer)==ESP_OK;return ready_;
}
bool EspNowLink::send(const Protocol::Message&m){uint8_t wire[Protocol::WIRE_SIZE];if(!ready_||!Protocol::encode(m,wire,sizeof(wire)))return false;esp_err_t result=esp_now_send(config_.peerMac,wire,sizeof(wire));if(result!=ESP_OK)++stats_.sendFailures;return result==ESP_OK;}
void EspNowLink::receiveCallback(const uint8_t* mac,const uint8_t* data,int length){if(instance_)instance_->enqueue(mac,data,length);}
void EspNowLink::sendCallback(const uint8_t*,esp_now_send_status_t status){if(!instance_)return;if(status==ESP_NOW_SEND_SUCCESS)++instance_->stats_.sent;else ++instance_->stats_.sendFailures;}
void EspNowLink::enqueue(const uint8_t* mac,const uint8_t* data,int length){
 if(!Protocol::authorizedSource(config_.peerMac,mac)){++stats_.unauthorized;return;}if(!data||length!=int(Protocol::WIRE_SIZE)){++stats_.invalidSize;return;}
 portENTER_CRITICAL(&queueMux);if(count_>=QUEUE_CAPACITY){++stats_.overflows;portEXIT_CRITICAL(&queueMux);return;}memcpy(queue_[tail_].data,data,Protocol::WIRE_SIZE);queue_[tail_].length=uint8_t(length);tail_=uint8_t((tail_+1)%QUEUE_CAPACITY);++count_;++stats_.queued;portEXIT_CRITICAL(&queueMux);
}
void EspNowLink::update(){for(;;){portENTER_CRITICAL(&queueMux);if(!count_){portEXIT_CRITICAL(&queueMux);break;}Frame f=queue_[head_];head_=uint8_t((head_+1)%QUEUE_CAPACITY);--count_;portEXIT_CRITICAL(&queueMux);Protocol::Message m{};if(Protocol::decode(f.data,f.length,m)!=Protocol::ValidationError::NONE){++stats_.invalidFrames;continue;}++stats_.received;if(callback_)callback_(m);}}
void EspNowLink::onReceive(std::function<void(const Protocol::Message&)>c){callback_=c;}bool EspNowLink::ready()const{return ready_;}const EspNowLink::Stats& EspNowLink::stats()const{return stats_;}const uint8_t* EspNowLink::peerMac()const{return config_.peerMac;}uint8_t EspNowLink::channel()const{return config_.channel;}
