#include "ControladorCerradura.h"

#include "config.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

ControladorCerradura::ControladorCerradura(bool simulado)
  : cerradura_(EstadoCerradura::DESCONOCIDA), modo_(Modo::AUTOMATICO),
    puerta_(Protocolo::EstadoPuerta::DESCONOCIDA), ultimaSenalSensor_(0),
    autocierreSegundos_(Config::TIEMPO_AUTOCIERRE_DEFECTO_S), inicioAutocierre_(0),
    secuenciaEvento_(0), sensorConDatos_(false), autocierreActivo_(false),
    simulado_(simulado) {
  credenciales_.agregar(Config::UID_TARJETA, Config::UID_TARJETA_LONGITUD,
                       Accion::DESBLOQUEAR, Config::ID_TARJETA);
  credenciales_.agregar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD,
                       Accion::DESBLOQUEAR, Config::ID_LLAVERO);
}

void ControladorCerradura::registrarEvento(std::function<void(const Evento&)> callback) {
  eventoCallback_ = callback;
}

void ControladorCerradura::registrarMovimiento(std::function<bool(Accion)> callback) {
  movimientoCallback_ = callback;
}

void ControladorCerradura::emitir(Origen origen, TipoEvento tipo, bool ejecutado,
                                const char* motivo, const char* credentialId) {
  Evento evento{};
  evento.seq = ++secuenciaEvento_;
  evento.origen = origen;
  evento.tipo = tipo;
  evento.resultado = ejecutado ? ResultadoEvento::EJECUTADO : ResultadoEvento::RECHAZADO;
  snprintf(evento.motivo, sizeof(evento.motivo), "%s", motivo);
  snprintf(evento.credentialId, sizeof(evento.credentialId), "%s",
           credentialId == nullptr ? "" : credentialId);
  evento.puerta = puerta();
  evento.cerradura = cerradura_;
  evento.modo = modo_;
  evento.simulado = simulado_;
  if (eventoCallback_) eventoCallback_(evento);
}

void ControladorCerradura::cancelarAutocierre() { autocierreActivo_ = false; }

bool ControladorCerradura::datosSensorRecientes() const {
  return sensorConDatos_ &&
    static_cast<uint32_t>(millis() - ultimaSenalSensor_) < Config::TIMEOUT_SENSOR_MS;
}

void ControladorCerradura::revisarVigencia() {
  if (!sensorConDatos_ || datosSensorRecientes()) return;
  sensorConDatos_ = false;
  puerta_ = Protocolo::EstadoPuerta::DESCONOCIDA;
  cancelarAutocierre();
  emitir(Origen::EDGE, TipoEvento::SENSOR, false, "sensor_offline");
}

void ControladorCerradura::evaluarAutocierre(bool reiniciar) {
  if (modo_ != Modo::AUTOMATICO || cerradura_ != EstadoCerradura::DESBLOQUEADA ||
      !datosSensorRecientes() || puerta_ != Protocolo::EstadoPuerta::CERRADA) {
    cancelarAutocierre();
    return;
  }
  if (reiniciar || !autocierreActivo_) {
    inicioAutocierre_ = millis();
    autocierreActivo_ = true;
    emitir(Origen::EDGE, TipoEvento::AUTOCIERRE, true, "temporizador_iniciado");
  }
}

bool ControladorCerradura::actualizarPuerta(Protocolo::EstadoPuerta nuevaPuerta, Origen origen) {
  revisarVigencia();
  if (nuevaPuerta != Protocolo::EstadoPuerta::ABIERTA &&
      nuevaPuerta != Protocolo::EstadoPuerta::CERRADA) return false;

  const bool recuperado = !sensorConDatos_;
  const bool cambio = puerta_ != nuevaPuerta;
  ultimaSenalSensor_ = millis();
  sensorConDatos_ = true;
  puerta_ = nuevaPuerta;
  if (recuperado) emitir(origen, TipoEvento::SENSOR, true, "sensor_online");
  if (cambio) emitir(origen, TipoEvento::PUERTA, true,
                    nuevaPuerta == Protocolo::EstadoPuerta::ABIERTA ? "puerta_abierta" : "puerta_cerrada");
  evaluarAutocierre(); // Refrescar una misma lectura no reinicia la cuenta.
  return true;
}

bool ControladorCerradura::recibirLatido(Protocolo::EstadoPuerta lectura, Origen origen) {
  return actualizarPuerta(lectura, origen);
}

Resultado ControladorCerradura::ejecutarAccion(Accion accion, Origen origen,
                                             const char* credentialId) {
  revisarVigencia();
  if (accion != Accion::BLOQUEAR && accion != Accion::DESBLOQUEAR)
    return {ResultadoEvento::RECHAZADO, "accion_invalida"};
  if (accion == Accion::DESBLOQUEAR && modo_ == Modo::BLOQUEADO)
    return {ResultadoEvento::RECHAZADO, "modo_bloqueado"};
  if (accion == Accion::BLOQUEAR) {
    if (!datosSensorRecientes()) return {ResultadoEvento::RECHAZADO, "sensor_sin_datos"};
    if (puerta_ != Protocolo::EstadoPuerta::CERRADA)
      return {ResultadoEvento::RECHAZADO, "puerta_no_cerrada"};
  }

  const EstadoCerradura destino = accion == Accion::BLOQUEAR
    ? EstadoCerradura::BLOQUEADA : EstadoCerradura::DESBLOQUEADA;
  if (cerradura_ == destino) return {ResultadoEvento::EJECUTADO, "sin_cambio"};
  if (!movimientoCallback_ || !movimientoCallback_(accion))
    return {ResultadoEvento::RECHAZADO, "orden_servo_fallida"};

  // El callback solo confirma emision de la orden. No hay sensor de pestillo.
  cerradura_ = destino;
  emitir(origen, accion == Accion::BLOQUEAR ? TipoEvento::BLOQUEO : TipoEvento::DESBLOQUEO,
         true, simulado_ ? "movimiento_simulado" : "orden_pwm_emitida", credentialId);
  evaluarAutocierre();
  return {ResultadoEvento::EJECUTADO, "ok"};
}

Resultado ControladorCerradura::procesarAccion(Accion accion, Origen origen) {
  const Resultado resultado = ejecutarAccion(accion, origen, nullptr);
  if (strcmp(resultado.motivo, "ok") != 0)
    emitir(origen, TipoEvento::SOLICITUD, resultado.resultado == ResultadoEvento::EJECUTADO,
           resultado.motivo);
  return resultado;
}

Resultado ControladorCerradura::procesarUid(const uint8_t* uid, uint8_t longitud, Origen origen) {
  revisarVigencia();
  Accion accion;
  const char* credentialId = nullptr;
  Resultado resultado{ResultadoEvento::RECHAZADO, "uid_no_registrado"};
  if (credenciales_.buscar(uid, longitud, accion, credentialId))
    resultado = ejecutarAccion(accion, origen, credentialId);
  // Un ACCESO por presentacion; BLOQUEO/DESBLOQUEO solo si hubo una nueva orden.
  emitir(origen, TipoEvento::ACCESO, resultado.resultado == ResultadoEvento::EJECUTADO,
         resultado.motivo, credentialId);
  return resultado;
}

Resultado ControladorCerradura::establecerModo(Modo nuevoModo, Origen origen) {
  revisarVigencia();
  if (nuevoModo != Modo::MANUAL && nuevoModo != Modo::AUTOMATICO && nuevoModo != Modo::BLOQUEADO) {
    emitir(origen, TipoEvento::MODO, false, "modo_invalido");
    return {ResultadoEvento::RECHAZADO, "modo_invalido"};
  }
  if (modo_ == nuevoModo) return {ResultadoEvento::EJECUTADO, "sin_cambio"};
  modo_ = nuevoModo;
  emitir(origen, TipoEvento::MODO, true, "modo_actualizado");
  evaluarAutocierre();
  return {ResultadoEvento::EJECUTADO, "modo_actualizado"};
}

Resultado ControladorCerradura::establecerAutocierre(uint32_t segundos, Origen origen) {
  revisarVigencia();
  if (segundos < Config::TIEMPO_AUTOCIERRE_MIN_S || segundos > Config::TIEMPO_AUTOCIERRE_MAX_S) {
    emitir(origen, TipoEvento::AUTOCIERRE, false, "autocierre_fuera_de_rango");
    return {ResultadoEvento::RECHAZADO, "autocierre_fuera_de_rango"};
  }
  if (autocierreSegundos_ == segundos) return {ResultadoEvento::EJECUTADO, "sin_cambio"};
  autocierreSegundos_ = segundos;
  emitir(origen, TipoEvento::AUTOCIERRE, true, "autocierre_actualizado");
  evaluarAutocierre(true);
  return {ResultadoEvento::EJECUTADO, "autocierre_actualizado"};
}

void ControladorCerradura::actualizar() {
  revisarVigencia();
  if (!autocierreActivo_) return;
  if (modo_ != Modo::AUTOMATICO || cerradura_ != EstadoCerradura::DESBLOQUEADA ||
      !datosSensorRecientes() || puerta_ != Protocolo::EstadoPuerta::CERRADA) {
    cancelarAutocierre();
    return;
  }
  if (static_cast<uint32_t>(millis() - inicioAutocierre_) >= autocierreSegundos_ * 1000UL) {
    cancelarAutocierre(); // Un fallo no produce reintentos en cada vuelta del loop.
    procesarAccion(Accion::BLOQUEAR, Origen::EDGE);
  }
}

EstadoCerradura ControladorCerradura::cerradura() const { return cerradura_; }
Modo ControladorCerradura::modo() const { return modo_; }
Protocolo::EstadoPuerta ControladorCerradura::puerta() const {
  return datosSensorRecientes() ? puerta_ : Protocolo::EstadoPuerta::DESCONOCIDA;
}
uint32_t ControladorCerradura::autocierreSegundos() const { return autocierreSegundos_; }
bool ControladorCerradura::sensorVigente() const { return datosSensorRecientes(); }
bool ControladorCerradura::autocierrePendiente() const {
  return autocierreActivo_ && datosSensorRecientes();
}
