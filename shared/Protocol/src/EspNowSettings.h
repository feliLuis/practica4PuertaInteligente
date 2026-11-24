#pragma once
#include "EspNowLink.h"
#if defined(__has_include)
#if __has_include("esp_now_config.h")
#include "esp_now_config.h"
#endif
#endif
#ifndef ESPNOW_PEER_CONFIGURED
#define ESPNOW_PEER_CONFIGURED 0
#define ESPNOW_PEER_MAC {0,0,0,0,0,0}
#define ESPNOW_CHANNEL 1
#define ESPNOW_ENCRYPTION_ENABLED 0
#define ESPNOW_PMK {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define ESPNOW_LMK {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#endif
inline EspNowLink::Config espNowSettings(){EspNowLink::Config c={ESPNOW_PEER_MAC,ESPNOW_PEER_CONFIGURED!=0,ESPNOW_CHANNEL,ESPNOW_ENCRYPTION_ENABLED!=0,ESPNOW_PMK,ESPNOW_LMK};return c;}
