#pragma once

#include <stdint.h>

namespace Config {
constexpr uint8_t UID_TARJETA[] = {0x39, 0x61, 0xD7, 0x06};
constexpr uint8_t UID_TARJETA_LONGITUD = sizeof(UID_TARJETA);
constexpr uint8_t UID_LLAVERO[] = {0x9C, 0x31, 0x8B, 0x06};
constexpr uint8_t UID_LLAVERO_LONGITUD = sizeof(UID_LLAVERO);
constexpr const char* ID_TARJETA = "tarjeta-1";
constexpr const char* ID_LLAVERO = "llavero-1";

constexpr uint8_t PIN_SERVO = 13;
constexpr int ANGULO_BLOQUEO = 0;
constexpr int ANGULO_DESBLOQUEO = 90;
constexpr uint32_t TIEMPO_AUTOCIERRE_DEFECTO_S = 10;
constexpr uint32_t TIEMPO_AUTOCIERRE_MIN_S = 5;
constexpr uint32_t TIEMPO_AUTOCIERRE_MAX_S = 120;
constexpr uint32_t TIMEOUT_SENSOR_MS = 30000;
constexpr uint32_t SERVO_DETACH_MS = 700;

constexpr uint32_t VELOCIDAD_SERIE = 115200;
constexpr uint8_t CAPACIDAD_COMANDO = 80;
}
