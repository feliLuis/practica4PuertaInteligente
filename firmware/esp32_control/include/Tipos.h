#pragma once

#include <stdint.h>
#include <Protocolo.h>

enum class Origen : uint8_t {
  VOZ,
  RFID,
  EDGE,
  NUBE,
  DIAGNOSTICO
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
  AUTOCIERRE,
  SOLICITUD // Rechazo, fallo o solicitud que no requiere movimiento.
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
  char motivo[48];
  Protocolo::EstadoPuerta puerta;
  EstadoCerradura cerradura; // Estimacion por orden PWM, nunca confirmacion fisica.
  Modo modo;
  char credentialId[24]; // Copia propia; nunca contiene el UID.
  uint32_t seq; // Local al arranque, no es identidad persistente para AWS.
  bool simulado;
};
