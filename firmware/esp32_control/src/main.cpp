#include <Arduino.h>
#include "config.h"
#include "Tipos.h"
#include "ControladorCerradura.h"
#include "MonitorSerie.h"

#ifndef MODO_SIMULACION
#define MODO_SIMULACION 0
#endif
#ifndef MODO_CALIBRACION
#define MODO_CALIBRACION 0
#endif
#if MODO_SIMULACION && MODO_CALIBRACION
#error "Simulacion y calibracion son configuraciones excluyentes."
#endif

#if !MODO_SIMULACION
#include "ServoCerrojo.h"
ServoCerrojo servo(Config::PIN_SERVO, Config::SERVO_DETACH_MS);
#endif
ControladorCerradura controlador(MODO_SIMULACION != 0);
MonitorSerie monitor;

const char* nombreTipo(TipoEvento tipo) {
  switch (tipo) {
    case TipoEvento::ACCESO: return "acceso";
    case TipoEvento::DESBLOQUEO: return "desbloqueo";
    case TipoEvento::BLOQUEO: return "bloqueo";
    case TipoEvento::PUERTA: return "puerta";
    case TipoEvento::SENSOR: return "sensor";
    case TipoEvento::MODO: return "modo";
    case TipoEvento::AUTOCIERRE: return "autocierre";
    case TipoEvento::SOLICITUD: return "solicitud";
  }
  return "desconocido";
}

const char* nombreOrigen(Origen origen) {
  switch (origen) {
    case Origen::VOZ: return "voz";
    case Origen::RFID: return "rfid";
    case Origen::EDGE: return "edge";
    case Origen::NUBE: return "nube";
    case Origen::DIAGNOSTICO: return "diagnostico";
  }
  return "desconocido";
}

const char* nombreCerradura(EstadoCerradura estado) {
  switch (estado) {
    case EstadoCerradura::BLOQUEADA: return "bloqueada";
    case EstadoCerradura::DESBLOQUEADA: return "desbloqueada";
    default: return "desconocida";
  }
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
  Serial.printf("[EVT%s] seq=%lu origen=%s tipo=%s resultado=%s motivo=%s "
                "puerta=%s cerradura_estimada=%s modo=%s credentialId=%s\n",
    evento.simulado ? "-SIM" : "", static_cast<unsigned long>(evento.seq),
    nombreOrigen(evento.origen), nombreTipo(evento.tipo),
    evento.ejecutado ? "ejecutado" : "rechazado", evento.motivo,
    Protocolo::nombre(evento.puerta), nombreCerradura(evento.cerradura),
    nombreModo(evento.modo), evento.credentialId);
}

void imprimirEstado() {
  Serial.printf("cerradura_estimada=%s puerta=%s modo=%s autocierre_s=%lu "
                "sensor=%s temporizador=%s\n",
    nombreCerradura(controlador.cerradura()), Protocolo::nombre(controlador.puerta()),
    nombreModo(controlador.modo()), static_cast<unsigned long>(controlador.autocierreSegundos()),
    controlador.sensorVigente() ? "online" : "offline",
    controlador.autocierrePendiente() ? "activo" : "inactivo");
}

void imprimirAyuda() {
  Serial.println("Comandos: ayuda | estado");
#if MODO_SIMULACION
  Serial.println("[SIM SIN PWM] puerta abierta|cerrada | latido | uid HEX");
  Serial.println("desbloquear | bloquear | modo manual|automatico|bloqueado | autocierre N");
  Serial.println("latido sin lectura no renueva Reed; use puerta abierta/cerrada para datos validos.");
#elif MODO_CALIBRACION
  Serial.println("[CALIBRACION FISICA] calibrar 0 | calibrar 90");
  Serial.println("Requiere alimentacion y recorrido verificados. No usar durante operacion.");
#else
  Serial.println("[NORMAL] Simulacion y calibracion deshabilitadas.");
#endif
}

void procesarComando(const char* linea) {
  if (strcmp(linea, "ayuda") == 0) imprimirAyuda();
  else if (strcmp(linea, "estado") == 0) imprimirEstado();
#if MODO_SIMULACION
  else if (strcmp(linea, "puerta abierta") == 0)
    controlador.actualizarPuerta(Protocolo::EstadoPuerta::ABIERTA, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "puerta cerrada") == 0)
    controlador.actualizarPuerta(Protocolo::EstadoPuerta::CERRADA, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "latido") == 0) {
    controlador.recibirLatido();
    Serial.println("[SIM] Latido sin lectura: no renueva la vigencia del Reed.");
  } else if (strcmp(linea, "desbloquear") == 0)
    controlador.procesarAccion(Accion::DESBLOQUEAR, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "bloquear") == 0)
    controlador.procesarAccion(Accion::BLOQUEAR, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "modo manual") == 0)
    controlador.establecerModo(Modo::MANUAL, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "modo automatico") == 0)
    controlador.establecerModo(Modo::AUTOMATICO, Origen::DIAGNOSTICO);
  else if (strcmp(linea, "modo bloqueado") == 0)
    controlador.establecerModo(Modo::BLOQUEADO, Origen::DIAGNOSTICO);
  else if (strncmp(linea, "autocierre ", 11) == 0) {
    uint32_t segundos;
    if (MonitorSerie::entero(linea + 11, segundos))
      controlador.establecerAutocierre(segundos, Origen::DIAGNOSTICO);
    else Serial.println("[RX] Entero decimal invalido.");
  } else if (strncmp(linea, "uid ", 4) == 0) {
    uint8_t uid[10];
    uint8_t longitud = 0;
    if (Protocolo::hexAUid(linea + 4, uid, sizeof(uid), longitud) &&
        (longitud == 4 || longitud == 7 || longitud == 10))
      controlador.procesarUid(uid, longitud, Origen::DIAGNOSTICO);
    else Serial.println("[RX] UID invalido: se requieren 4, 7 o 10 bytes hexadecimales.");
  }
#elif MODO_CALIBRACION
  else if (strcmp(linea, "calibrar 0") == 0 || strcmp(linea, "calibrar 90") == 0) {
    const bool emitida = strcmp(linea, "calibrar 0") == 0 ? servo.bloquear() : servo.desbloquear();
    Serial.println(emitida ? "[CAL] Orden PWM emitida; posicion fisica sin confirmar."
                           : "[CAL] Fallo al emitir orden PWM.");
  }
#endif
  else Serial.println("[RX] Comando desconocido o deshabilitado en esta compilacion.");
}

void setup() {
  Serial.begin(Config::VELOCIDAD_SERIE);
  controlador.registrarEvento(registrarEvento);
#if MODO_SIMULACION
  controlador.registrarMovimiento([](Accion) {
    Serial.println("[SIM] Orden de movimiento simulada, sin PWM.");
    return true;
  });
#elif MODO_CALIBRACION
  if (!servo.begin()) Serial.println("[BOOT] Error de configuracion del servo.");
#else
  if (!servo.begin()) Serial.println("[BOOT] Error de configuracion del servo.");
  controlador.registrarMovimiento([](Accion accion) {
    return accion == Accion::DESBLOQUEAR ? servo.desbloquear() : servo.bloquear();
  });
#endif
  Serial.println("[BOOT] Puerta y cerradura desconocidas. Sin movimiento de arranque.");
  Serial.println("[BOOT] Sin comunicacion entre placas ni integracion AWS.");
  imprimirAyuda();
}

void loop() {
  // Presupuesto por vuelta: la entrada continua tampoco impide actualizar temporizadores.
  for (uint8_t i = 0; i < 64 && Serial.available(); ++i) {
    const auto resultado = monitor.recibir(static_cast<char>(Serial.read()));
    if (resultado == MonitorSerie::Resultado::LISTA) procesarComando(monitor.linea());
    else if (resultado == MonitorSerie::Resultado::INVALIDA)
      Serial.println("[RX] Linea descartada: excede capacidad o contiene caracteres invalidos.");
  }
  controlador.actualizar();
#if !MODO_SIMULACION
  servo.actualizar();
#endif
}
