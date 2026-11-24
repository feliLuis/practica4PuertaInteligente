#if defined(AWS_TELEMETRY_ENABLED) && AWS_TELEMETRY_ENABLED
#include "AwsTelemetry.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <inttypes.h>
#include <sys/time.h>

#if __has_include("aws_telemetry_config.h")
#include "aws_telemetry_config.h"
#else
#include "aws_telemetry_config.example.h"
#endif

namespace {
constexpr uint32_t REPORT_MS = 30000;
constexpr uint32_t OBSERVATION_TIMEOUT_MS = 1000;
constexpr uint32_t WIFI_RETRY_MS = 30000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000;
constexpr int64_t MIN_EPOCH_MS = 1704067200000LL;
constexpr int OUTBOX_LIMIT_BYTES = 4096;
constexpr size_t EVENT_CAPACITY = 24;

int64_t epochMs() {
  timeval clock{};
  gettimeofday(&clock, nullptr);
  const int64_t ms = static_cast<int64_t>(clock.tv_sec) * 1000 + clock.tv_usec / 1000;
  return ms >= MIN_EPOCH_MS ? ms : 0;
}

void makeEventId(char (&id)[47], int64_t timestamp) {
  uint32_t random[4];
  esp_fill_random(random, sizeof(random));
  snprintf(id, sizeof(id), "%013" PRId64 "#%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32,
           timestamp, random[0], random[1], random[2], random[3]);
}
}

bool AwsTelemetry::begin(uint8_t espNowChannel) {
  if (!AwsSettings::ENABLED) return false;
  const auto& bssid = AwsSettings::AP_BSSID;
  bool validBssid = false;
  for (auto octet : bssid) validBssid |= octet != 0;
  if (!validBssid || (bssid[0] & 1) || espNowChannel < 1 || espNowChannel > 13 ||
      !AwsSettings::WIFI_SSID[0] || strncmp(AwsSettings::MQTT_URI, "mqtts://", 8) != 0 ||
      !AwsSettings::THING_NAME[0] || !AwsSettings::ROOT_CA[0] ||
      !AwsSettings::CLIENT_CERT[0] || !AwsSettings::CLIENT_KEY[0]) {
    Serial.println("[AWS] Invalid local telemetry configuration.");
    return false;
  }
  for (char character : AwsSettings::THING_NAME) {
    if (character == '\0') break;
    if (!isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_')
      return false;
  }
  int eventLength = snprintf(eventTopic_, sizeof(eventTopic_), "chapa/%s/events", AwsSettings::THING_NAME);
  int shadowLength = snprintf(shadowTopic_, sizeof(shadowTopic_), "$aws/things/%s/shadow/update",
                              AwsSettings::THING_NAME);
  if (eventLength < 0 || static_cast<size_t>(eventLength) >= sizeof(eventTopic_) ||
      shadowLength < 0 || static_cast<size_t>(shadowLength) >= sizeof(shadowTopic_)) return false;

  channel_ = espNowChannel;
  events_ = xQueueCreate(EVENT_CAPACITY, sizeof(Pending));
  snapshots_ = xQueueCreate(1, sizeof(Observation));
  if (events_ && snapshots_ && xTaskCreate(worker, "aws-telemetry", 6144, this, 1, nullptr) == pdPASS) {
    enabled_ = true;
    return true;
  }
  if (events_) vQueueDelete(events_);
  if (snapshots_) vQueueDelete(snapshots_);
  return false;
}

void AwsTelemetry::record(const Event& event, uint32_t autoLockSeconds) {
  if (!enabled_ || event.simulated || event.source == Source::DIAGNOSTIC) return;
  Pending pending{event, autoLockSeconds, epochMs(), esp_timer_get_time() / 1000};
  if (xQueueSend(events_, &pending, 0) != pdTRUE) ++dropped_;
}

void AwsTelemetry::observe(const AwsPayload::Snapshot& snapshot) {
  const uint32_t now = millis();
  if (!enabled_ || now - lastObservation_ < 100) return;
  lastObservation_ = now;
  const Observation observation{snapshot, now};
  xQueueOverwrite(snapshots_, &observation);
}

void AwsTelemetry::worker(void* instance) {
  auto& self = *static_cast<AwsTelemetry*>(instance);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  self.lastAttempt_ = millis() - WIFI_RETRY_MS;
  for (;;) {
    self.tick();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool AwsTelemetry::wifiReady(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (WiFi.channel() == channel_) {
      connecting_ = false;
      return true;
    }
    Serial.println("[AWS] AP channel mismatch. Telemetry paused; keeping ESP-NOW channel.");
    WiFi.disconnect(false, false);
    connecting_ = false;
    lastAttempt_ = now;
    return false;
  }

  if (connecting_ && now - lastAttempt_ >= WIFI_CONNECT_TIMEOUT_MS) {
    WiFi.disconnect(false, false);
    connecting_ = false;
  }
  if (!connecting_) {
    uint8_t activeChannel;
    wifi_second_chan_t secondary;
    if (esp_wifi_get_channel(&activeChannel, &secondary) == ESP_OK && activeChannel != channel_)
      esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE);
    if (now - lastAttempt_ >= WIFI_RETRY_MS) {
      lastAttempt_ = now;
      connecting_ = true;
      WiFi.begin(AwsSettings::WIFI_SSID, AwsSettings::WIFI_PASSWORD, channel_, AwsSettings::AP_BSSID);
    }
  }
  return false;
}

void AwsTelemetry::mqttEvent(void* instance, esp_event_base_t, int32_t event, void*) {
  auto& self = *static_cast<AwsTelemetry*>(instance);
  if (event == MQTT_EVENT_CONNECTED) {
    self.connected_.store(true);
    self.refresh_.store(true);
    Serial.println("[AWS] MQTT/TLS connected (publish only).");
  } else if (event == MQTT_EVENT_DISCONNECTED) {
    self.connected_.store(false);
    Serial.println("[AWS] MQTT disconnected.");
  } else if (event == MQTT_EVENT_DELETED) {
    ++self.dropped_;
  }
}

bool AwsTelemetry::startMqtt() {
  esp_mqtt_client_config_t config{};
  config.uri = AwsSettings::MQTT_URI;
  config.client_id = AwsSettings::THING_NAME;
  config.cert_pem = AwsSettings::ROOT_CA;
  config.client_cert_pem = AwsSettings::CLIENT_CERT;
  config.client_key_pem = AwsSettings::CLIENT_KEY;
  config.reconnect_timeout_ms = 10000;
  config.network_timeout_ms = 4000;
  mqtt_ = esp_mqtt_client_init(&config);
  if (!mqtt_) return false;
  if (esp_mqtt_client_register_event(mqtt_, MQTT_EVENT_ANY, mqttEvent, this) == ESP_OK &&
      esp_mqtt_client_start(mqtt_) == ESP_OK) return true;
  esp_mqtt_client_destroy(mqtt_);
  mqtt_ = nullptr;
  return false;
}

bool AwsTelemetry::enqueue(const char* topic, const char* payload) {
  return esp_mqtt_client_get_outbox_size(mqtt_) < OUTBOX_LIMIT_BYTES &&
         esp_mqtt_client_enqueue(mqtt_, topic, payload, 0, 1, 0, true) >= 0;
}

void AwsTelemetry::tick() {
  if (xQueueReceive(snapshots_, &observation_, 0) == pdTRUE) hasSnapshot_ = true;
  const uint32_t drops = dropped_.load();
  if (drops != loggedDrops_) {
    loggedDrops_ = drops;
    Serial.printf("[AWS] Dropped telemetry: %lu\n", static_cast<unsigned long>(drops));
  }
  const uint32_t now = millis();
  if (!wifiReady(now)) return;
  if (!timeRequested_) {
    configTime(0, 0, "pool.ntp.org");
    timeRequested_ = true;
  }
  const int64_t utc = epochMs();
  if (!utc) return;
  if (!mqtt_) {
    static uint32_t lastStart = now - 10000;
    if (now - lastStart >= 10000) {
      lastStart = now;
      if (!startMqtt()) Serial.println("[AWS] MQTT startup failed.");
    }
    return;
  }
  if (!connected_.load()) return;
  if (refresh_.exchange(false)) {
    reported_ = false;
    lastReading_ = now - REPORT_MS;
  }

  char payload[384];
  const bool fresh = hasSnapshot_ && now - observation_.observedAt <= OBSERVATION_TIMEOUT_MS;
  const auto& snapshot = observation_.snapshot;
  if (fresh && (!reported_ || now - lastReport_ >= REPORT_MS || !AwsPayload::sameState(snapshot, latestReport_)) &&
      AwsPayload::reported(payload, sizeof(payload), snapshot, utc) && enqueue(shadowTopic_, payload)) {
    latestReport_ = snapshot;
    lastReport_ = now;
    reported_ = true;
  }

  char id[47];
  Pending pending{};
  if (xQueuePeek(events_, &pending, 0) == pdTRUE) {
    const int64_t age = esp_timer_get_time() / 1000 - pending.capturedUptimeMs;
    // Before SNTP, reconstruct capture time from the monotonic boot clock.
    makeEventId(id, pending.capturedMs ? pending.capturedMs : utc - age);
    if (!AwsPayload::event(payload, sizeof(payload), pending.event, pending.autoLockSeconds, id, age > 1000)) {
      xQueueReceive(events_, &pending, 0);
      ++dropped_;
    } else if (enqueue(eventTopic_, payload)) {
      xQueueReceive(events_, &pending, 0);
    }
  } else if (fresh && now - lastReading_ >= REPORT_MS) {
    makeEventId(id, utc);
    if (AwsPayload::reading(payload, sizeof(payload), snapshot, id) && enqueue(eventTopic_, payload))
      lastReading_ = now;
  }
}
#endif
