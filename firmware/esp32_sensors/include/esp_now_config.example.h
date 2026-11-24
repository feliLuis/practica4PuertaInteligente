#pragma once
// Copy to esp_now_config.h (ignored by Git), then set the control WiFi STA MAC.
#define ESPNOW_PEER_CONFIGURED 0
#define ESPNOW_PEER_MAC {0x00,0x00,0x00,0x00,0x00,0x00}
#define ESPNOW_CHANNEL 1
#define ESPNOW_ENCRYPTION_ENABLED 0
// Replace both arrays locally when encryption is enabled. Never commit real keys.
#define ESPNOW_PMK {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define ESPNOW_LMK {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
