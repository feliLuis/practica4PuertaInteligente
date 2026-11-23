#include "IndicadorAcceso.h"

#include "config.h"

IndicadorAcceso::IndicadorAcceso(uint8_t pinLedVerde, uint8_t pinLedRojo, uint8_t pinBuzzer, bool buzzerActivo)
  : ledVerde_(pinLedVerde), ledRojo_(pinLedRojo),
    buzzer_(pinBuzzer, buzzerActivo) {}

void IndicadorAcceso::begin() {
  ledVerde_.begin();
  ledRojo_.begin();
  buzzer_.begin();
}

void IndicadorAcceso::accesoAceptado() {
  ledRojo_.apagar();
  ledVerde_.encenderPor(Config::DURACION_LED_MS);
  buzzer_.apagar();
}

void IndicadorAcceso::accesoDenegado() {
  ledVerde_.apagar();
  ledRojo_.encenderPor(Config::DURACION_LED_MS);
  buzzer_.sonarPatron(
    Config::BUZZER_ENCENDIDO_MS,
    Config::BUZZER_APAGADO_MS,
    Config::BUZZER_REPETICIONES);
}

void IndicadorAcceso::testLedVerde() {
  ledRojo_.apagar();
  buzzer_.apagar();
  ledVerde_.encenderPor(Config::DURACION_LED_MS);
}

void IndicadorAcceso::testLedRojo() {
  ledVerde_.apagar();
  buzzer_.apagar();
  ledRojo_.encenderPor(Config::DURACION_LED_MS);
}

void IndicadorAcceso::testBuzzer() {
  buzzer_.sonarPatron(
    Config::BUZZER_ENCENDIDO_MS,
    Config::BUZZER_APAGADO_MS,
    Config::BUZZER_REPETICIONES);
}

void IndicadorAcceso::actualizar() {
  ledVerde_.actualizar();
  ledRojo_.actualizar();
  buzzer_.actualizar();
}
