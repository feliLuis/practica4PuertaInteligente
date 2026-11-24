#pragma once
#include "LocalLink.h"
#include <esp_now.h>
#include <stdint.h>

class EspNowLink : public LocalLink {
public:
 static constexpr uint8_t QUEUE_CAPACITY=6;
 struct Config { uint8_t peerMac[6]; bool peerConfigured; uint8_t channel; bool encryptionEnabled; uint8_t pmk[16]; uint8_t lmk[16]; };
 struct Stats { uint32_t queued,received,sent,sendFailures,unauthorized,invalidSize,invalidFrames,overflows; };
 explicit EspNowLink(const Config& config);
 bool begin() override; bool send(const Protocol::Message& message) override;
 void onReceive(std::function<void(const Protocol::Message&)> callback) override;
 void update() override; bool ready() const override;
 const Stats& stats() const; const uint8_t* peerMac() const; uint8_t channel() const;
 static bool validPeerMac(const uint8_t* mac);
private:
 struct Frame { uint8_t data[Protocol::WIRE_SIZE]; uint8_t length; };
 static EspNowLink* instance_;
 static void receiveCallback(const uint8_t* mac,const uint8_t* data,int length);
 static void sendCallback(const uint8_t* mac,esp_now_send_status_t status);
 void enqueue(const uint8_t* mac,const uint8_t* data,int length);
 Config config_;Stats stats_;Frame queue_[QUEUE_CAPACITY];volatile uint8_t head_,tail_,count_;bool ready_;
 std::function<void(const Protocol::Message&)> callback_;
};
