#include "Soporte.h"
// Se compila el main real contra dispositivos falsos, una vez por configuracion.
#include "../firmware/esp32_control/src/main.cpp"

void entrada(const std::string& texto) {
  for (char c : texto) Serial.entrada.push_back(c);
  while (Serial.available()) loop();
}
int main() {
  Suite s;
  s.caso("consola separa normal, simulacion y calibracion", [] {
    setup();
    COMPROBAR(Falso::escrituras == 0);
    entrada("puerta cerrada\nuid 3961D706\n");
#if MODO_SIMULACION
    COMPROBAR(controlador.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(Serial.salida.find("EVT-SIM") != std::string::npos);
    entrada("uid 9C318B06\nuid 3961D706\n");
    COMPROBAR(controlador.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(Serial.salida.find("credentialId=llavero-1") != std::string::npos);
    entrada("bloquear\nuid 9C318B06\n");
    COMPROBAR(controlador.cerradura() == EstadoCerradura::DESBLOQUEADA);
    entrada("modo bloqueado\nuid 3961D706\nuid 9C318B06\n");
    COMPROBAR(controlador.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(Serial.salida.find("resultado=rechazado motivo=modo_bloqueado") != std::string::npos);
#else
    COMPROBAR(controlador.cerradura() == EstadoCerradura::DESCONOCIDA);
    COMPROBAR(!controlador.sensorVigente());
#endif
    COMPROBAR(Falso::escrituras == 0);
    entrada("calibrar 0\ncalibrar 90\n");
#if MODO_CALIBRACION
    COMPROBAR(Falso::escrituras == 2 && Falso::ultimoAngulo == 90);
#else
    COMPROBAR(Falso::escrituras == 0);
#endif
    const int anteriores = Falso::escrituras;
    entrada("servo 0\ncalibrar 45\ncalibrar abc\ncalibrar 0x\n");
    COMPROBAR(Falso::escrituras == anteriores);
#if MODO_SIMULACION
    entrada("autocierre 10x\n"); COMPROBAR(controlador.autocierreSegundos() == 10);
    entrada("autocierre 5\n"); COMPROBAR(controlador.autocierreSegundos() == 5);
    entrada("autocierre 4294967296\n"); COMPROBAR(controlador.autocierreSegundos() == 5);
#endif
    entrada(std::string(100, 'x') + "calibrar 0\n");
    COMPROBAR(Falso::escrituras == anteriores);
    entrada("ESTADO\r\n");
    COMPROBAR(Serial.salida.find("cerradura_estimada=") != std::string::npos);
  });
  return s.resultado();
}
