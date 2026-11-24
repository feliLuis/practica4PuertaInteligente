#pragma once

#include <stdint.h>

#ifndef DIAGNOSTICO_UID
#define DIAGNOSTICO_UID 0
#endif

namespace Config {
constexpr uint8_t RC522_SS = 21;
constexpr uint8_t RC522_SCK = 18;
constexpr uint8_t RC522_MOSI = 23;
constexpr uint8_t RC522_MISO = 19;
constexpr uint8_t RC522_RST = 22;

constexpr uint8_t PIN_REED = 27;
constexpr uint32_t DEBOUNCE_MS = 100;
constexpr uint32_t LATIDO_MS = 10000;
constexpr uint32_t ANTIRREPETICION_RFID_MS = 1500;
constexpr uint32_t REINTENTO_RFID_MS = 5000;
constexpr uint32_t COMPROBACION_RFID_MS = 1000;
constexpr uint32_t SONDEO_RFID_MS = 50;
constexpr bool REED_CERRADO_ES_LOW = true;
constexpr bool MOSTRAR_UID_COMPLETO = DIAGNOSTICO_UID != 0;
constexpr uint32_t VELOCIDAD_SERIE = 115200;
}
