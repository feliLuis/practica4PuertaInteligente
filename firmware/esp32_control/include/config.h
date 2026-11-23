#pragma once

#include <stdint.h>

namespace Config {
constexpr uint8_t UID_DESBLOQUEAR[] = {0x39, 0x61, 0xD7, 0x06};
constexpr uint8_t UID_DESBLOQUEAR_LONGITUD = sizeof(UID_DESBLOQUEAR);
constexpr uint8_t UID_BLOQUEAR[] = {0x9C, 0x31, 0x8B, 0x06};
constexpr uint8_t UID_BLOQUEAR_LONGITUD = sizeof(UID_BLOQUEAR);

constexpr uint8_t PIN_SERVO = 13;
constexpr int ANGULO_BLOQUEO = 60;
constexpr int ANGULO_DESBLOQUEO = 120;
constexpr uint32_t TIEMPO_AUTOCIERRE_DEFECTO_S = 10;
constexpr uint32_t TIEMPO_AUTOCIERRE_MIN_S = 5;
constexpr uint32_t TIEMPO_AUTOCIERRE_MAX_S = 120;
constexpr uint32_t TIMEOUT_SENSOR_MS = 30000;
constexpr uint32_t SERVO_DETACH_MS = 700;

constexpr uint8_t PIN_LED_VERDE = 25;
constexpr uint8_t PIN_LED_ROJO = 26;
constexpr uint8_t PIN_BUZZER = 33;
constexpr bool BUZZER_ACTIVO = true;
constexpr uint32_t DURACION_LED_MS = 2000;
constexpr uint32_t BUZZER_ENCENDIDO_MS = 150;
constexpr uint32_t BUZZER_APAGADO_MS = 100;
constexpr uint8_t BUZZER_REPETICIONES = 3;

constexpr uint32_t VELOCIDAD_SERIE = 115200;
}
