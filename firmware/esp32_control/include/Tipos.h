#pragma once

#include <stdint.h>

enum class Origen : uint8_t {
  VOZ,
  RFID,
  EDGE,
  NUBE
};

enum class EstadoCerradura : uint8_t {
  DESCONOCIDA,
  BLOQUEADA,
  DESBLOQUEADA
};

enum class Modo : uint8_t {
  MANUAL,
  AUTOMATICO,
  BLOQUEADO
};

enum class Accion : uint8_t {
  BLOQUEAR,
  DESBLOQUEAR
};

enum class TipoEvento : uint8_t {
  ACCESO,
  DESBLOQUEO,
  BLOQUEO,
  PUERTA,
  SENSOR,
  MODO,
  AUTOCIERRE
};

enum class ResultadoEvento : uint8_t {
  EJECUTADO,
  RECHAZADO
};

struct Resultado {
  ResultadoEvento resultado;
  const char* motivo;
};

struct Evento {
  Origen origen;
  TipoEvento tipo;
  ResultadoEvento resultado;
  bool ejecutado;
  const char* motivo;
};
