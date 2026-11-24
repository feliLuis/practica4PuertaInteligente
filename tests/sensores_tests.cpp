#include "soporte_sensores/Soporte.h"
#include "SensorPuerta.h"
#include "LectorRFID.h"
#include "EnlaceSerial.h"
#include "config.h"
#include <cstring>

using Puerta = Protocolo::EstadoPuerta;
void presentar(byte longitud = 4) {
  Falso::tarjetaPresente = true;
  Falso::uidLen = longitud;
  for (byte i = 0; i < 10; ++i) Falso::uid[i] = i + 1;
}
int main() {
  Suite s;
  s.caso("Reed inicial desconocido hasta 100 ms", [] {
    Falso::nivel[27] = LOW; SensorPuerta sensor; sensor.begin();
    COMPROBAR(Falso::modo[27] == INPUT_PULLUP && sensor.estado() == Puerta::DESCONOCIDA);
    Falso::ahora = 99; COMPROBAR(!sensor.actualizar());
    Falso::ahora = 100; COMPROBAR(sensor.actualizar() && sensor.estado() == Puerta::CERRADA);
    COMPROBAR(!sensor.actualizar());
  });
  s.caso("rebotes iniciales reinician estabilizacion", [] {
    Falso::nivel[27] = LOW; SensorPuerta sensor; sensor.begin();
    Falso::ahora = 50; Falso::nivel[27] = HIGH; COMPROBAR(!sensor.actualizar());
    Falso::ahora = 149; COMPROBAR(!sensor.actualizar());
    Falso::ahora = 150; COMPROBAR(sensor.actualizar() && sensor.estado() == Puerta::ABIERTA);
  });
  s.caso("cambios de Reed estables sin eventos redundantes", [] {
    SensorPuerta sensor; sensor.begin(); Falso::ahora = 100; sensor.actualizar();
    Falso::nivel[27] = LOW; Falso::ahora = 110; COMPROBAR(!sensor.actualizar());
    Falso::nivel[27] = HIGH; Falso::ahora = 140; COMPROBAR(!sensor.actualizar());
    Falso::nivel[27] = LOW; Falso::ahora = 160; COMPROBAR(!sensor.actualizar());
    Falso::ahora = 259; COMPROBAR(!sensor.actualizar());
    ++Falso::ahora; COMPROBAR(sensor.actualizar() && sensor.estado() == Puerta::CERRADA);
    COMPROBAR(!sensor.actualizar());
  });
  s.caso("Reed debounce al desbordar millis", [] {
    Falso::ahora = UINT32_MAX - 50; SensorPuerta sensor; sensor.begin();
    Falso::ahora += 99; COMPROBAR(!sensor.actualizar());
    ++Falso::ahora; COMPROBAR(sensor.actualizar());
  });
  s.caso("RFID conserva pines SPI", [] {
    LectorRFID lector(21, 22); COMPROBAR(lector.begin());
    COMPROBAR(Falso::spi[0] == 18 && Falso::spi[1] == 19 &&
              Falso::spi[2] == 23 && Falso::spi[3] == 21);
  });
  s.caso("RFID fallo inicial y reintento cada 5 s", [] {
    Falso::versionRfid = 0; LectorRFID lector(21, 22); COMPROBAR(!lector.begin());
    Falso::versionRfid = 0x92; Falso::ahora = 4999; lector.actualizar();
    COMPROBAR(!lector.disponible() && Falso::inicializacionesRfid == 1);
    Falso::ahora = 5000; lector.actualizar();
    COMPROBAR(lector.disponible() && Falso::inicializacionesRfid == 2);
  });
  s.caso("RFID perdida posterior y recuperacion espaciada", [] {
    LectorRFID lector(21, 22); lector.begin(); Falso::versionRfid = 0xFF;
    Falso::ahora = 999; lector.actualizar(); COMPROBAR(lector.disponible());
    Falso::ahora = 1000; lector.actualizar(); COMPROBAR(!lector.disponible());
    Falso::versionRfid = 0x92; Falso::ahora = 5999; lector.actualizar();
    COMPROBAR(Falso::inicializacionesRfid == 1); ++Falso::ahora; lector.actualizar();
    COMPROBAR(lector.disponible() && Falso::inicializacionesRfid == 2);
  });
  s.caso("RFID longitudes validas y capacidad insuficiente", [] {
    LectorRFID lector(21, 22); lector.begin(); uint8_t uid[10]; uint8_t longitud = 99;
    Falso::ahora = 50; presentar(3); COMPROBAR(!lector.leer(uid, sizeof(uid), longitud) && longitud == 0);
    Falso::ahora += 50; presentar(11); COMPROBAR(!lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora += 50; presentar(7); COMPROBAR(!lector.leer(uid, 4, longitud));
    Falso::ahora += 50; presentar(7); COMPROBAR(lector.leer(uid, sizeof(uid), longitud) && longitud == 7);
    Falso::ahora += 50; presentar(10); COMPROBAR(lector.leer(uid, sizeof(uid), longitud) && longitud == 10);
    Falso::ahora += 50; presentar(4); COMPROBAR(lector.leer(uid, sizeof(uid), longitud) && longitud == 4);
  });
  s.caso("RFID antirrepeticion temporal", [] {
    LectorRFID lector(21, 22); lector.begin(); uint8_t uid[10]; uint8_t longitud;
    Falso::ahora = 50; presentar(); COMPROBAR(lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora = 100; presentar(); COMPROBAR(!lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora = 1500; presentar(); COMPROBAR(!lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora = 1550; presentar(); COMPROBAR(lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora += 50; presentar(); Falso::uid[0] = 99;
    COMPROBAR(lector.leer(uid, sizeof(uid), longitud));
  });
  s.caso("RFID antirrepeticion al desbordar millis", [] {
    LectorRFID lector(21, 22); lector.begin(); uint8_t uid[10]; uint8_t longitud;
    Falso::ahora = UINT32_MAX - 1000; presentar(); COMPROBAR(lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora += 1499; presentar(); COMPROBAR(!lector.leer(uid, sizeof(uid), longitud));
    Falso::ahora += 51; presentar(); COMPROBAR(lector.leer(uid, sizeof(uid), longitud));
  });
  s.caso("simulador serial respeta privacidad y estado de puerta", [] {
    EnlaceSerial enlace; COMPROBAR(enlace.begin());
    Protocolo::Mensaje mensaje{}; mensaje.version = Protocolo::VERSION;
    mensaje.tipo = Protocolo::Tipo::RFID_PRESENTADA; mensaje.uidLen = 4;
    mensaje.uid[0] = 0xAA; mensaje.uid[1] = 0xBB; mensaje.uid[2] = 0xCC; mensaje.uid[3] = 0xDD;
    mensaje.puerta = Puerta::CERRADA; COMPROBAR(enlace.enviar(mensaje));
    COMPROBAR(Serial.salida.find("SIN ENLACE") != std::string::npos);
    COMPROBAR(Serial.salida.find("puerta=cerrada") != std::string::npos);
    COMPROBAR((Serial.salida.find("AABBCCDD") != std::string::npos) == Config::MOSTRAR_UID_COMPLETO);
    mensaje.uidLen = 11; COMPROBAR(!enlace.enviar(mensaje));
  });
  return s.resultado();
}
