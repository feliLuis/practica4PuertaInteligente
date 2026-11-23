#pragma once

#include "Tipos.h"
#include <Protocolo.h>
#include "ListaCredenciales.h"
#include <functional>
#include <stdint.h>

class ControladorCerradura {
public:
  ControladorCerradura();

  void registrarEvento(std::function<void(const Evento&)> callback);
  void registrarMovimiento(std::function<void(Accion)> callback);
  void actualizar();
  void actualizarPuerta(Protocolo::EstadoPuerta puerta);
  void recibirLatido();
  Resultado procesarAccion(Accion accion, Origen origen);
  Resultado procesarUid(const uint8_t* uid, uint8_t longitud);
  Resultado establecerModo(Modo modo);
  Resultado establecerAutocierre(uint32_t segundos);

  EstadoCerradura cerradura() const;
  Modo modo() const;
  Protocolo::EstadoPuerta puerta() const;
  uint32_t autocierreSegundos() const;
  bool sensorVigente() const;

private:
  void emitir(Origen origen, TipoEvento tipo, bool ejecutado, const char* motivo);
  void cancelarAutocierre();
  bool datosSensorRecientes() const;

  ListaCredenciales credenciales_;
  std::function<void(const Evento&)> eventoCallback_;
  std::function<void(Accion)> movimientoCallback_;
  EstadoCerradura cerradura_;
  Modo modo_;
  Protocolo::EstadoPuerta puerta_;
  uint32_t ultimaSenalSensor_;
  uint32_t autocierreSegundos_;
  uint32_t inicioAutocierre_;
  bool sensorConDatos_;
  bool sensorPerdidoEmitido_;
  bool esperandoCicloPuerta_;
  bool vioPuertaAbierta_;
  bool autocierreActivo_;
};
