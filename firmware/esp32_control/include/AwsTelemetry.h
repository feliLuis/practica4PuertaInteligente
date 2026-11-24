#pragma once

#include "AwsTelemetryPayload.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

class AwsTelemetry {
public:
  bool begin(uint8_t espNowChannel);
  void record(const Event& event, uint32_t autoLockSeconds);
  void observe(const AwsPayload::Snapshot& snapshot);

private:
  struct Pending {
    Event event;
    uint32_t autoLockSeconds;
    int64_t capturedMs;
    int64_t capturedUptimeMs;
  };
  struct Observation {
    AwsPayload::Snapshot snapshot;
    uint32_t observedAt;
  };

  static void worker(void* instance);
  static void mqttEvent(void* instance, esp_event_base_t base, int32_t event, void* data);
  void tick();
  bool wifiReady(uint32_t now);
  bool startMqtt();
  bool enqueue(const char* topic, const char* payload);

  QueueHandle_t events_ = nullptr;
  QueueHandle_t snapshots_ = nullptr;
  esp_mqtt_client_handle_t mqtt_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> refresh_{false};
  std::atomic<uint32_t> dropped_{0};
  uint32_t loggedDrops_ = 0;
  uint32_t lastAttempt_ = 0;
  uint32_t lastReport_ = 0;
  uint32_t lastReading_ = 0;
  uint32_t lastObservation_ = 0;
  uint8_t channel_ = 0;
  bool enabled_ = false;
  bool connecting_ = false;
  bool timeRequested_ = false;
  bool hasSnapshot_ = false;
  bool reported_ = false;
  Observation observation_{};
  AwsPayload::Snapshot latestReport_{};
  char eventTopic_[96]{};
  char shadowTopic_[112]{};
};
