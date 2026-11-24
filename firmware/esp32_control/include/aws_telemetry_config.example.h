#pragma once

namespace AwsSettings {
constexpr bool ENABLED = false;
constexpr char WIFI_SSID[] = "";
constexpr char WIFI_PASSWORD[] = "";
// Match the BSSID and fixed 2.4 GHz channel of the AP used by ESP-NOW.
constexpr unsigned char AP_BSSID[6] = {};
constexpr char MQTT_URI[] = ""; // mqtts://<ATS endpoint>:8883
constexpr char THING_NAME[] = "chapa-inteligente-01";
constexpr char ROOT_CA[] = "";
constexpr char CLIENT_CERT[] = "";
constexpr char CLIENT_KEY[] = "";
}
