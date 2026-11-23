#include <Arduino.h>
#include "config.h"
#include "Tipos.h"
#include "ControladorCerradura.h"
#include "ServoCerrojo.h"
#include "IndicadorAcceso.h"
#include <Protocolo.h>

ControladorCerradura controlador;
ServoCerrojo servo(Config::PIN_SERVO, Config::SERVO_DETACH_MS);
IndicadorAcceso indicador(
  Config::PIN_LED_VERDE,
  Config::PIN_LED_ROJO,
  Config::PIN_BUZZER,
  Config::BUZZER_ACTIVO);

const char* nombreTipo(TipoEvento tipo) {
  switch (tipo) {
    case TipoEvento::ACCESO: return "acceso";
    case TipoEvento::DESBLOQUEO: return "desbloqueo";
    case TipoEvento::BLOQUEO: return "bloqueo";
    case TipoEvento::PUERTA: return "puerta";
    case TipoEvento::SENSOR: return "sensor";
    case TipoEvento::MODO: return "modo";
    case TipoEvento::AUTOCIERRE: return "autocierre";
  }
  return "desconocido";
}

const char* nombreOrigen(Origen origen) {
  switch (origen) {
    case Origen::VOZ: return "voz";
    case Origen::RFID: return "rfid";
    case Origen::EDGE: return "edge";
    case Origen::NUBE: return "nube";
  }
  return "desconocido";
}

const char* nombreCerradura(EstadoCerradura estado) {
  switch (estado) {
    case EstadoCerradura::DESCONOCIDA: return "desconocida";
    case EstadoCerradura::BLOQUEADA: return "bloqueada";
    case EstadoCerradura::DESBLOQUEADA: return "desbloqueada";
  }
  return "desconocida";
}

const char* nombreModo(Modo modo) {
  switch (modo) {
    case Modo::MANUAL: return "manual";
    case Modo::AUTOMATICO: return "automatico";
    case Modo::BLOQUEADO: return "bloqueado";
  }
  return "desconocido";
}

void registrarEvento(const Evento& evento) {
  Serial.printf("[EVT] origen=%s tipo=%s ejecutado=%s motivo=%s\n",
    nombreOrigen(evento.origen),
    nombreTipo(evento.tipo),
    evento.ejecutado ? "si" : "no",
    evento.motivo);

  if (evento.origen == Origen::RFID) {
    if (evento.tipo == TipoEvento::DESBLOQUEO ||
        evento.tipo == TipoEvento::BLOQUEO ||
        evento.tipo == TipoEvento::ACCESO) {
      if (evento.ejecutado) {
        indicador.accesoAceptado();
      } else {
        indicador.accesoDenegado();
      }
    }
  }

}

void imprimirEstado() {
  Serial.println("================================");
  Serial.printf("cerradura=%s\n", nombreCerradura(controlador.cerradura()));
  Serial.printf("puerta=%s\n", Protocolo::nombre(controlador.puerta()));
  Serial.printf("modo=%s\n", nombreModo(controlador.modo()));
  Serial.printf("autocierre=%lu s\n",
    static_cast<unsigned long>(controlador.autocierreSegundos()));
  Serial.printf("sensor=%s\n", controlador.sensorVigente() ? "vigente" : "sin_datos");
  Serial.println("================================");
}

void imprimirAyuda() {
  Serial.println("Comandos:");
  Serial.println("  puerta abierta|cerrada");
  Serial.println("  uid 3961D706");
  Serial.println("  latido");
  Serial.println("  desbloquear");
  Serial.println("  bloquear");
  Serial.println("  modo manual|automatico|bloqueado");
  Serial.println("  autocierre N");
  Serial.println("  servo N");
  Serial.println("  test verde");
  Serial.println("  test rojo");
  Serial.println("  test buzzer");
  Serial.println("  estado");
  Serial.println("  ayuda");
}

String obtenerArgumento(const String& linea, uint8_t inicio) {
  String argumento = linea.substring(inicio);
  argumento.trim();
  argumento.toUpperCase();
  return argumento;
}

void procesarComando(String linea) {
  linea.trim();
  linea.toLowerCase();
  if (linea.length() == 0) return;

  if (linea == "ayuda") {
    imprimirAyuda();
  } else if (linea == "estado") {
    imprimirEstado();
  } else if (linea == "puerta abierta") {
    controlador.actualizarPuerta(Protocolo::EstadoPuerta::ABIERTA);
  } else if (linea == "puerta cerrada") {
    controlador.actualizarPuerta(Protocolo::EstadoPuerta::CERRADA);
  } else if (linea == "latido") {
    controlador.recibirLatido();
    Serial.println("[SIM] latido recibido");
  } else if (linea == "desbloquear") {
    controlador.procesarAccion(Accion::DESBLOQUEAR, Origen::VOZ);
  } else if (linea == "bloquear") {
    controlador.procesarAccion(Accion::BLOQUEAR, Origen::VOZ);
  } else if (linea == "modo manual") {
    controlador.establecerModo(Modo::MANUAL);
  } else if (linea == "modo automatico") {
    controlador.establecerModo(Modo::AUTOMATICO);
  } else if (linea == "modo bloqueado") {
    controlador.establecerModo(Modo::BLOQUEADO);
  } else if (linea.startsWith("autocierre ")) {
    int segundos = linea.substring(11).toInt();
    controlador.establecerAutocierre(static_cast<uint32_t>(segundos));
  } else if (linea.startsWith("servo ")) {
    int angulo = linea.substring(6).toInt();
    servo.moverA(angulo);
    Serial.printf("[CAL] servo movido a %d grados\n", angulo);
  } else if (linea == "test verde") {
    indicador.testLedVerde();
  } else if (linea == "test rojo") {
    indicador.testLedRojo();
  } else if (linea == "test buzzer") {
    indicador.testBuzzer();
  } else if (linea.startsWith("uid ")) {
    String texto = linea.substring(4);
    texto.trim();
    uint8_t uid[10];
    uint8_t longitud = 0;
    if (Protocolo::hexAUid(texto.c_str(), uid, 10, longitud)) {
      controlador.procesarUid(uid, longitud);
    } else {
      Serial.println("[RX] UID invalido");
    }
  } else {
    Serial.println("[RX] Comando desconocido. Escriba 'ayuda'.");
  }
}

void setup() {
  Serial.begin(Config::VELOCIDAD_SERIE);
  Serial.setTimeout(50);
  delay(100);
  servo.begin();
  indicador.begin();
  controlador.registrarEvento(registrarEvento);
  controlador.registrarMovimiento([](Accion accion) {
    if (accion == Accion::DESBLOQUEAR) servo.desbloquear();
    else servo.bloquear();
  });

  Serial.println("[BOOT] ESP32 #1 control listo.");
  Serial.println("[BOOT] Cerradura desconocida hasta movimiento confirmado por el controlador.");
  Serial.println("[BOOT] Use 'latido' o 'puerta abierta/cerrada' para simular el ESP32 #2.");
  imprimirAyuda();
}

void loop() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    procesarComando(linea);
  }
  controlador.actualizar();
  servo.actualizar();
  indicador.actualizar();
}
