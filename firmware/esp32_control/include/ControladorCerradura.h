#pragma once

#include "Tipos.h"
#include "ListaCredenciales.h"
#include <functional>

class ControladorCerradura {
public:
  explicit ControladorCerradura(bool simulado = false);

  void registrarEvento(std::function<void(const Evento&)> callback);
  // true confirma emision de la orden, no que el servo alcanzo su posicion.
  void registrarMovimiento(std::function<bool(Accion)> callback);
  void actualizar();
  bool actualizarPuerta(Protocolo::EstadoPuerta puerta, Origen origen = Origen::EDGE);
  // Un latido sin lectura valida no renueva la vigencia del Reed.
  bool recibirLatido(Protocolo::EstadoPuerta puerta = Protocolo::EstadoPuerta::DESCONOCIDA,
                     Origen origen = Origen::EDGE);
  Resultado procesarAccion(Accion accion, Origen origen);
  Resultado procesarUid(const uint8_t* uid, uint8_t longitud, Origen origen = Origen::RFID);
  Resultado establecerModo(Modo modo, Origen origen);
  Resultado establecerAutocierre(uint32_t segundos, Origen origen);

  EstadoCerradura cerradura() const;
  Modo modo() const;
  Protocolo::EstadoPuerta puerta() const;
  uint32_t autocierreSegundos() const;
  bool sensorVigente() const;
  bool autocierrePendiente() const;

private:
  Resultado ejecutarAccion(Accion accion, Origen origen, const char* credentialId);
  void emitir(Origen origen, TipoEvento tipo, bool ejecutado, const char* motivo,
              const char* credentialId = nullptr);
  void cancelarAutocierre();
  void evaluarAutocierre(bool reiniciar = false);
  void revisarVigencia();
  bool datosSensorRecientes() const;

  ListaCredenciales credenciales_;
  std::function<void(const Evento&)> eventoCallback_;
  std::function<bool(Accion)> movimientoCallback_;
  EstadoCerradura cerradura_;
  Modo modo_;
  Protocolo::EstadoPuerta puerta_;
  uint32_t ultimaSenalSensor_;
  uint32_t autocierreSegundos_;
  uint32_t inicioAutocierre_;
  uint32_t secuenciaEvento_;
  bool sensorConDatos_;
  bool autocierreActivo_;
  bool simulado_;
};
