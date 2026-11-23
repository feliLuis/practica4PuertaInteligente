#include "ControladorCerradura.h"

#include "config.h"
#include <Arduino.h>

ControladorCerradura::ControladorCerradura()
  : cerradura_(EstadoCerradura::DESCONOCIDA),
    modo_(Modo::AUTOMATICO),
    puerta_(Protocolo::EstadoPuerta::DESCONOCIDA),
    ultimaSenalSensor_(0),
    autocierreSegundos_(Config::TIEMPO_AUTOCIERRE_DEFECTO_S),
    inicioAutocierre_(0),
    sensorConDatos_(false),
    sensorPerdidoEmitido_(false),
    esperandoCicloPuerta_(false),
    vioPuertaAbierta_(false),
    autocierreActivo_(false) {

  credenciales_.agregar(Config::UID_DESBLOQUEAR, Config::UID_DESBLOQUEAR_LONGITUD, Accion::DESBLOQUEAR);
  credenciales_.agregar(Config::UID_BLOQUEAR, Config::UID_BLOQUEAR_LONGITUD, Accion::BLOQUEAR);
}

void ControladorCerradura::registrarEvento(std::function<void(const Evento&)> callback) {
  eventoCallback_ = callback;
}

void ControladorCerradura::registrarMovimiento(std::function<void(Accion)> callback) {
  movimientoCallback_ = callback;
}

void ControladorCerradura::emitir(Origen origen, TipoEvento tipo, bool ejecutado, const char* motivo) {
  if (!eventoCallback_) return;
  Evento evento;
  evento.origen = origen;
  evento.tipo = tipo;
  evento.resultado = ejecutado ? ResultadoEvento::EJECUTADO : ResultadoEvento::RECHAZADO;
  evento.ejecutado = ejecutado;
  evento.motivo = motivo;
  eventoCallback_(evento);
}

void ControladorCerradura::cancelarAutocierre() {
  autocierreActivo_ = false;
  inicioAutocierre_ = 0;
}

bool ControladorCerradura::datosSensorRecientes() const {
  if (!sensorConDatos_) return false;
  return static_cast<uint32_t>(millis() - ultimaSenalSensor_) < Config::TIMEOUT_SENSOR_MS;
}

void ControladorCerradura::recibirLatido() {
  ultimaSenalSensor_ = millis();
  sensorConDatos_ = true;
  if (sensorPerdidoEmitido_) {
    sensorPerdidoEmitido_ = false;
  }
}

void ControladorCerradura::actualizarPuerta(Protocolo::EstadoPuerta nuevaPuerta) {
  recibirLatido();

  if (puerta_ == nuevaPuerta) return;
  const Protocolo::EstadoPuerta anterior = puerta_;
  puerta_ = nuevaPuerta;
  emitir(Origen::EDGE, TipoEvento::PUERTA,
    nuevaPuerta != Protocolo::EstadoPuerta::DESCONOCIDA,
    nuevaPuerta == Protocolo::EstadoPuerta::ABIERTA ? "puerta_abierta" :
    nuevaPuerta == Protocolo::EstadoPuerta::CERRADA ? "puerta_cerrada" : "puerta_desconocida");

  if (nuevaPuerta == Protocolo::EstadoPuerta::ABIERTA) {
    vioPuertaAbierta_ = esperandoCicloPuerta_;
    cancelarAutocierre();
  } else if (nuevaPuerta == Protocolo::EstadoPuerta::CERRADA) {
    if (modo_ == Modo::AUTOMATICO &&
        cerradura_ == EstadoCerradura::DESBLOQUEADA &&
        esperandoCicloPuerta_ && vioPuertaAbierta_ && datosSensorRecientes()) {
      inicioAutocierre_ = millis();
      autocierreActivo_ = true;
      emitir(Origen::EDGE, TipoEvento::AUTOCIERRE, true, "temporizador_iniciado");
    }
  } else if (anterior != Protocolo::EstadoPuerta::DESCONOCIDA) {
    cancelarAutocierre();
  }
}

Resultado ControladorCerradura::procesarAccion(Accion accion, Origen origen) {
  Resultado resultado;

  if (accion == Accion::DESBLOQUEAR) {
    if (modo_ == Modo::BLOQUEADO) {
      resultado = {ResultadoEvento::RECHAZADO, "modo_bloqueado"};
      emitir(origen, TipoEvento::DESBLOQUEO, false, "modo_bloqueado");
      return resultado;
    }
    cerradura_ = EstadoCerradura::DESBLOQUEADA;
    if (modo_ == Modo::AUTOMATICO) {
      esperandoCicloPuerta_ = true;
      vioPuertaAbierta_ = (puerta_ == Protocolo::EstadoPuerta::ABIERTA);
    }
    cancelarAutocierre();
    resultado = {ResultadoEvento::EJECUTADO, "ok"};
    emitir(origen, TipoEvento::DESBLOQUEO, true, "ok");
    if (movimientoCallback_) movimientoCallback_(Accion::DESBLOQUEAR);
    return resultado;
  }

  if (!datosSensorRecientes()) {
    resultado = {ResultadoEvento::RECHAZADO, "sensor_sin_datos"};
    emitir(origen, TipoEvento::BLOQUEO, false, "sensor_sin_datos");
  } else if (puerta_ == Protocolo::EstadoPuerta::ABIERTA) {
    resultado = {ResultadoEvento::RECHAZADO, "puerta_abierta"};
    emitir(origen, TipoEvento::BLOQUEO, false, "puerta_abierta");
  } else if (puerta_ == Protocolo::EstadoPuerta::DESCONOCIDA) {
    resultado = {ResultadoEvento::RECHAZADO, "puerta_desconocida"};
    emitir(origen, TipoEvento::BLOQUEO, false, "puerta_desconocida");
  } else {
    cerradura_ = EstadoCerradura::BLOQUEADA;
    esperandoCicloPuerta_ = false;
    vioPuertaAbierta_ = false;
    cancelarAutocierre();
    resultado = {ResultadoEvento::EJECUTADO, "ok"};
    emitir(origen, TipoEvento::BLOQUEO, true, "ok");
    if (movimientoCallback_) movimientoCallback_(Accion::BLOQUEAR);
  }
  return resultado;
}

Resultado ControladorCerradura::procesarUid(const uint8_t* uid, uint8_t longitud) {
  Accion accion;
  if (!credenciales_.buscar(uid, longitud, accion)) {
    Resultado resultado = {ResultadoEvento::RECHAZADO, "uid_no_registrado"};
    emitir(Origen::RFID, TipoEvento::ACCESO, false, "uid_no_registrado");
    return resultado;
  }
  Resultado resultado = procesarAccion(accion, Origen::RFID);

  return resultado;
}

Resultado ControladorCerradura::establecerModo(Modo nuevoModo) {
  modo_ = nuevoModo;
  if (nuevoModo != Modo::AUTOMATICO) {
    esperandoCicloPuerta_ = false;
    vioPuertaAbierta_ = false;
    cancelarAutocierre();
  }
  Resultado resultado = {ResultadoEvento::EJECUTADO, "modo_actualizado"};
  emitir(Origen::VOZ, TipoEvento::MODO, true, "modo_actualizado");
  return resultado;
}

Resultado ControladorCerradura::establecerAutocierre(uint32_t segundos) {
  if (segundos < Config::TIEMPO_AUTOCIERRE_MIN_S ||
      segundos > Config::TIEMPO_AUTOCIERRE_MAX_S) {
    Resultado resultado = {ResultadoEvento::RECHAZADO, "autocierre_fuera_de_rango"};
    emitir(Origen::VOZ, TipoEvento::AUTOCIERRE, false, "autocierre_fuera_de_rango");
    return resultado;
  }
  autocierreSegundos_ = segundos;
  Resultado resultado = {ResultadoEvento::EJECUTADO, "autocierre_actualizado"};
  emitir(Origen::VOZ, TipoEvento::AUTOCIERRE, true, "autocierre_actualizado");
  return resultado;
}

void ControladorCerradura::actualizar() {
  if (sensorConDatos_ && !datosSensorRecientes() && !sensorPerdidoEmitido_) {
    sensorConDatos_ = false;
    sensorPerdidoEmitido_ = true;
    puerta_ = Protocolo::EstadoPuerta::DESCONOCIDA;
    esperandoCicloPuerta_ = false;
    vioPuertaAbierta_ = false;
    cancelarAutocierre();
    emitir(Origen::EDGE, TipoEvento::SENSOR, false, "sensor_sin_datos");
  }

  if (autocierreActivo_ && datosSensorRecientes() &&
      puerta_ == Protocolo::EstadoPuerta::CERRADA) {
    if (static_cast<uint32_t>(millis() - inicioAutocierre_) >= autocierreSegundos_ * 1000UL) {
      autocierreActivo_ = false;
      procesarAccion(Accion::BLOQUEAR, Origen::EDGE);
    }
  } else if (autocierreActivo_ && puerta_ != Protocolo::EstadoPuerta::CERRADA) {
    cancelarAutocierre();
  }
}

EstadoCerradura ControladorCerradura::cerradura() const {
  return cerradura_;
}

Modo ControladorCerradura::modo() const {
  return modo_;
}

Protocolo::EstadoPuerta ControladorCerradura::puerta() const {
  return puerta_;
}

uint32_t ControladorCerradura::autocierreSegundos() const {
  return autocierreSegundos_;
}

bool ControladorCerradura::sensorVigente() const {
  return datosSensorRecientes();
}
