#include "Soporte.h"
#include "ControladorCerradura.h"
#include "ServoCerrojo.h"
#include "MonitorSerie.h"
#include "config.h"
#include <vector>
#include <cstring>

using Puerta = Protocolo::EstadoPuerta;
const Origen diagnostico = Origen::DIAGNOSTICO;
struct Banco {
  ControladorCerradura c{true};
  std::vector<Evento> eventos;
  int movimientos = 0;
  bool emitirOrden = true;
  Banco() {
    c.registrarEvento([this](const Evento& e) { eventos.push_back(e); });
    c.registrarMovimiento([this](Accion) { ++movimientos; return emitirOrden; });
  }
  void cerrar() { c.actualizarPuerta(Puerta::CERRADA, diagnostico); }
  void abrir() { c.actualizarPuerta(Puerta::ABIERTA, diagnostico); }
  Resultado desbloquear() { return c.procesarAccion(Accion::DESBLOQUEAR, diagnostico); }
  Resultado bloquear() { return c.procesarAccion(Accion::BLOQUEAR, diagnostico); }
  size_t contar(TipoEvento tipo) const {
    size_t total = 0;
    for (const auto& e : eventos) if (e.tipo == tipo) ++total;
    return total;
  }
};
bool aceptado(Resultado r) { return r.resultado == ResultadoEvento::EJECUTADO; }

int main() {
  Suite s;
  s.caso("arranque desconocido sin movimientos", [] {
    Banco b; COMPROBAR(b.c.cerradura() == EstadoCerradura::DESCONOCIDA);
    COMPROBAR(b.c.puerta() == Puerta::DESCONOCIDA);
    COMPROBAR(b.c.modo() == Modo::AUTOMATICO); COMPROBAR(b.c.autocierreSegundos() == 10);
    COMPROBAR(b.movimientos == 0 && !b.c.sensorVigente());
  });
  s.caso("bloqueo cerrado vigente", [] {
    Banco b; b.cerrar(); COMPROBAR(aceptado(b.bloquear()));
    COMPROBAR(b.movimientos == 1 && b.c.cerradura() == EstadoCerradura::BLOQUEADA);
  });
  s.caso("rechazo puerta abierta", [] {
    Banco b; b.abrir(); COMPROBAR(!aceptado(b.bloquear())); COMPROBAR(b.movimientos == 0);
  });
  s.caso("rechazo desconocida y valores invalidos", [] {
    Banco b; COMPROBAR(!aceptado(b.bloquear()));
    COMPROBAR(!b.c.actualizarPuerta(static_cast<Puerta>(255)));
    COMPROBAR(!b.c.recibirLatido()); COMPROBAR(!b.c.sensorVigente());
    COMPROBAR(!aceptado(b.c.procesarAccion(static_cast<Accion>(255), diagnostico)));
    COMPROBAR(b.movimientos == 0);
  });
  s.caso("rechazo timeout incluso sin actualizar previo", [] {
    Banco b; b.cerrar(); Falso::ahora = 30000;
    COMPROBAR(b.c.puerta() == Puerta::DESCONOCIDA);
    COMPROBAR(!aceptado(b.bloquear())); COMPROBAR(b.movimientos == 0);
  });
  s.caso("tarjeta autoriza y conserva identificador logico", [] {
    Banco b; const uint8_t uid[] = {0x39, 0x61, 0xD7, 0x06};
    COMPROBAR(std::memcmp(uid, Config::UID_TARJETA, sizeof(uid)) == 0);
    COMPROBAR(aceptado(b.c.procesarUid(uid, sizeof(uid))));
    COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(b.contar(TipoEvento::ACCESO) == 1);
    COMPROBAR(b.contar(TipoEvento::DESBLOQUEO) == 1);
    COMPROBAR(std::strcmp(b.eventos.back().credentialId, "tarjeta-1") == 0);
  });
  s.caso("llavero desbloquea sin requerir lectura y conserva identificador", [] {
    Banco b; const uint8_t uid[] = {0x9C, 0x31, 0x8B, 0x06};
    COMPROBAR(std::memcmp(uid, Config::UID_LLAVERO, sizeof(uid)) == 0);
    COMPROBAR(aceptado(b.c.procesarUid(uid, sizeof(uid))));
    COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(std::strcmp(b.eventos.back().credentialId, "llavero-1") == 0);
    COMPROBAR(b.movimientos == 1 && b.contar(TipoEvento::ACCESO) == 1);
    COMPROBAR(b.contar(TipoEvento::DESBLOQUEO) == 1 && b.contar(TipoEvento::BLOQUEO) == 0);
  });
  s.caso("lista impide registrar bloqueo incluso al reemplazar credencial", [] {
    ListaCredenciales lista;
    COMPROBAR(!lista.agregar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD,
                            Accion::BLOQUEAR, Config::ID_LLAVERO));
    Accion accion = Accion::BLOQUEAR; const char* id = nullptr;
    COMPROBAR(!lista.buscar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD, accion, id));
    COMPROBAR(lista.agregar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD,
                           Accion::DESBLOQUEAR, Config::ID_LLAVERO));
    COMPROBAR(!lista.agregar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD,
                            Accion::BLOQUEAR, "otro-id"));
    COMPROBAR(!lista.agregar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD,
                            static_cast<Accion>(255), Config::ID_LLAVERO));
    COMPROBAR(lista.buscar(Config::UID_LLAVERO, Config::UID_LLAVERO_LONGITUD, accion, id));
    COMPROBAR(accion == Accion::DESBLOQUEAR && std::strcmp(id, "llavero-1") == 0);
  });
  s.caso("ambas credenciales desbloquean a 90 en manual y automatico sin alternar", [] {
    const uint8_t* uids[] = {Config::UID_TARJETA, Config::UID_LLAVERO};
    for (Modo modo : {Modo::MANUAL, Modo::AUTOMATICO}) {
      for (const uint8_t* uid : uids) {
        Banco b; ServoCerrojo servo(Config::PIN_SERVO, Config::SERVO_DETACH_MS);
        COMPROBAR(servo.begin()); b.c.establecerModo(modo, diagnostico); b.cerrar();
        b.c.registrarMovimiento([&](Accion accion) {
          ++b.movimientos;
          return accion == Accion::DESBLOQUEAR ? servo.desbloquear() : servo.bloquear();
        });
        COMPROBAR(aceptado(b.bloquear()) && Falso::ultimoAngulo == 0);
        const int previas = Falso::escrituras;
        COMPROBAR(aceptado(b.c.procesarUid(uid, 4)));
        COMPROBAR(Falso::ultimoAngulo == 90 && Falso::escrituras == previas + 1);
        COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
        for (const uint8_t* repetida : uids) COMPROBAR(aceptado(b.c.procesarUid(repetida, 4)));
        COMPROBAR(Falso::escrituras == previas + 1 && b.movimientos == 2);
        COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
        COMPROBAR(b.contar(TipoEvento::DESBLOQUEO) == 1 && b.contar(TipoEvento::BLOQUEO) == 1);
        COMPROBAR(b.contar(TipoEvento::ACCESO) == 3);
      }
    }
  });
  s.caso("modo bloqueado rechaza ambas credenciales desde cualquier estado sin movimientos", [] {
    const uint8_t* uids[] = {Config::UID_TARJETA, Config::UID_LLAVERO};
    const char* ids[] = {"tarjeta-1", "llavero-1"};
    for (int inicial = 0; inicial < 3; ++inicial) {
      Banco b; b.cerrar();
      if (inicial == 1) b.bloquear();
      if (inicial == 2) b.desbloquear();
      const auto estado = b.c.cerradura(); const int previos = b.movimientos;
      const auto bloqueos = b.contar(TipoEvento::BLOQUEO);
      const auto desbloqueos = b.contar(TipoEvento::DESBLOQUEO);
      b.c.establecerModo(Modo::BLOQUEADO, diagnostico);
      for (int i = 0; i < 2; ++i) {
        const auto r = b.c.procesarUid(uids[i], 4);
        COMPROBAR(!aceptado(r) && std::strcmp(r.motivo, "modo_bloqueado") == 0);
        COMPROBAR(std::strcmp(b.eventos.back().credentialId, ids[i]) == 0);
        COMPROBAR(b.eventos.back().resultado == ResultadoEvento::RECHAZADO);
      }
      COMPROBAR(b.movimientos == previos && b.c.cerradura() == estado);
      COMPROBAR(b.contar(TipoEvento::ACCESO) == 2 && !b.c.autocierrePendiente());
      COMPROBAR(b.contar(TipoEvento::BLOQUEO) == bloqueos);
      COMPROBAR(b.contar(TipoEvento::DESBLOQUEO) == desbloqueos);
    }
  });
  s.caso("repetir o alternar credenciales no mueve ni prolonga autocierre", [] {
    for (const uint8_t* inicial : {Config::UID_TARJETA, Config::UID_LLAVERO}) {
      Falso::ahora = 0; Banco b; b.cerrar();
      COMPROBAR(aceptado(b.c.procesarUid(inicial, 4)));
      Falso::ahora = 9000;
      for (const uint8_t* uid : {Config::UID_TARJETA, Config::UID_LLAVERO, inicial}) {
        const auto r = b.c.procesarUid(uid, 4);
        COMPROBAR(aceptado(r) && std::strcmp(r.motivo, "sin_cambio") == 0);
      }
      COMPROBAR(b.movimientos == 1 && b.contar(TipoEvento::DESBLOQUEO) == 1);
      COMPROBAR(b.contar(TipoEvento::ACCESO) == 4 && b.contar(TipoEvento::BLOQUEO) == 0);
      Falso::ahora = 9999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
      Falso::ahora = 10000; b.c.actualizar();
      COMPROBAR(b.movimientos == 2 && b.c.cerradura() == EstadoCerradura::BLOQUEADA);
    }
  });
  s.caso("UID desconocido un acceso sin movimiento", [] {
    Banco b; const uint8_t uid[] = {1, 2, 3, 4};
    COMPROBAR(!aceptado(b.c.procesarUid(uid, 4)));
    COMPROBAR(!aceptado(b.c.procesarUid(nullptr, 4)));
    COMPROBAR(b.movimientos == 0 && b.eventos.size() == 2);
    COMPROBAR(b.contar(TipoEvento::ACCESO) == 2 && b.eventos[0].credentialId[0] == '\0');
  });
  s.caso("autocierre puerta inicialmente cerrada", [] {
    Banco b; b.cerrar(); b.desbloquear(); COMPROBAR(b.c.autocierrePendiente());
    Falso::ahora = 9999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    Falso::ahora = 10000; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
    COMPROBAR(b.c.cerradura() == EstadoCerradura::BLOQUEADA);
  });
  s.caso("autocierre espera cierre de puerta abierta", [] {
    Banco b; b.abrir(); b.desbloquear(); Falso::ahora = 15000; b.c.actualizar();
    COMPROBAR(b.movimientos == 1 && !b.c.autocierrePendiente());
    b.cerrar(); Falso::ahora += 9999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    ++Falso::ahora; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
  });
  s.caso("reapertura cancela y reinicia periodo completo", [] {
    Banco b; b.cerrar(); b.desbloquear(); Falso::ahora = 7000; b.abrir();
    Falso::ahora = 8000; b.cerrar(); Falso::ahora = 10000; b.c.actualizar();
    COMPROBAR(b.movimientos == 1); Falso::ahora = 17999; b.c.actualizar();
    COMPROBAR(b.movimientos == 1); ++Falso::ahora; b.c.actualizar();
    COMPROBAR(b.movimientos == 2);
  });
  s.caso("offline cancela; recuperacion valida inicia periodo completo", [] {
    Banco b; b.c.establecerAutocierre(40, diagnostico); b.cerrar(); b.desbloquear();
    Falso::ahora = 30000; b.c.actualizar(); b.c.actualizar();
    COMPROBAR(!b.c.autocierrePendiente() && b.c.puerta() == Puerta::DESCONOCIDA);
    COMPROBAR(b.contar(TipoEvento::SENSOR) == 2); // online inicial y offline
    COMPROBAR(!b.c.recibirLatido() && !b.c.sensorVigente());
    Falso::ahora = 31000; COMPROBAR(b.c.recibirLatido(Puerta::CERRADA));
    b.c.recibirLatido(Puerta::CERRADA); COMPROBAR(b.contar(TipoEvento::SENSOR) == 3);
    Falso::ahora = 51000; b.c.recibirLatido(Puerta::CERRADA);
    Falso::ahora = 70999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    ++Falso::ahora; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
  });
  s.caso("mensaje invalido no renueva vigencia", [] {
    Banco b; b.cerrar(); Falso::ahora = 29000;
    COMPROBAR(!b.c.actualizarPuerta(static_cast<Puerta>(99)));
    COMPROBAR(!b.c.recibirLatido()); Falso::ahora = 30000;
    COMPROBAR(!aceptado(b.bloquear()));
  });
  s.caso("manual no autocierra; automatico evalua inmediatamente", [] {
    Banco b; b.c.establecerModo(Modo::MANUAL, diagnostico); b.cerrar(); b.desbloquear();
    Falso::ahora = 11000; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    b.c.establecerModo(Modo::AUTOMATICO, diagnostico);
    Falso::ahora = 20999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    ++Falso::ahora; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
  });
  s.caso("bloqueado cancela sin forzar bloqueo y rechaza desbloqueo", [] {
    Banco b; b.cerrar(); b.desbloquear(); b.abrir();
    b.c.establecerModo(Modo::BLOQUEADO, diagnostico);
    COMPROBAR(!aceptado(b.desbloquear())); COMPROBAR(!b.c.autocierrePendiente());
    Falso::ahora = 10000; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
    COMPROBAR(!aceptado(b.c.establecerModo(static_cast<Modo>(255), diagnostico)));
    COMPROBAR(b.c.modo() == Modo::BLOQUEADO);
  });
  s.caso("repetir desbloqueo no mueve ni reinicia cuenta", [] {
    Banco b; b.cerrar(); b.desbloquear(); Falso::ahora = 9000; b.desbloquear();
    b.c.recibirLatido(Puerta::CERRADA); Falso::ahora = 10000; b.c.actualizar();
    COMPROBAR(b.movimientos == 2);
    b.bloquear(); COMPROBAR(b.movimientos == 2 && b.contar(TipoEvento::BLOQUEO) == 1);
  });
  s.caso("rango y cambio de tiempo reinicia periodo completo", [] {
    Banco b;
    COMPROBAR(!aceptado(b.c.establecerAutocierre(4, diagnostico)));
    COMPROBAR(!aceptado(b.c.establecerAutocierre(121, diagnostico)));
    COMPROBAR(!aceptado(b.c.establecerAutocierre(UINT32_MAX, diagnostico)));
    COMPROBAR(aceptado(b.c.establecerAutocierre(120, diagnostico)));
    b.cerrar(); b.desbloquear(); Falso::ahora = 9000;
    COMPROBAR(aceptado(b.c.establecerAutocierre(5, diagnostico)));
    Falso::ahora = 13999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    ++Falso::ahora; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
  });
  s.caso("fallo del servo conserva estado y no informa movimiento", [] {
    Banco b; b.cerrar(); b.emitirOrden = false;
    COMPROBAR(!aceptado(b.bloquear()));
    COMPROBAR(b.c.cerradura() == EstadoCerradura::DESCONOCIDA);
    COMPROBAR(b.contar(TipoEvento::BLOQUEO) == 0);
    COMPROBAR(std::strcmp(b.eventos.back().motivo, "orden_servo_fallida") == 0);
    b.c.registrarMovimiento(nullptr); COMPROBAR(!aceptado(b.desbloquear()));
  });
  s.caso("fallo de autocierre no reintenta cada loop", [] {
    Banco b; b.cerrar(); b.desbloquear(); b.emitirOrden = false;
    Falso::ahora = 10000; b.c.actualizar(); b.c.actualizar(); b.c.actualizar();
    COMPROBAR(b.movimientos == 2 && !b.c.autocierrePendiente());
    COMPROBAR(b.c.cerradura() == EstadoCerradura::DESBLOQUEADA);
  });
  s.caso("callback de movimiento precede a estado y evento", [] {
    ControladorCerradura c;
    c.registrarMovimiento([&](Accion) {
      COMPROBAR(c.cerradura() == EstadoCerradura::DESCONOCIDA); return true;
    });
    COMPROBAR(aceptado(c.procesarAccion(Accion::DESBLOQUEAR, diagnostico)));
    COMPROBAR(c.cerradura() == EstadoCerradura::DESBLOQUEADA);
  });
  s.caso("eventos son copias con secuencia y marca de simulacion", [] {
    Banco b; b.cerrar(); b.desbloquear(); const auto copia = b.eventos;
    b.abrir(); b.c.establecerModo(Modo::MANUAL, diagnostico);
    for (size_t i = 0; i < b.eventos.size(); ++i) {
      COMPROBAR(b.eventos[i].seq == i + 1 && b.eventos[i].simulado);
    }
    COMPROBAR(copia[0].puerta == Puerta::CERRADA && copia[0].modo == Modo::AUTOMATICO);
    COMPROBAR(copia[0].origen == diagnostico && std::strcmp(copia[0].motivo, "sensor_online") == 0);
  });
  s.caso("millis desborda durante autocierre y vigencia", [] {
    Falso::ahora = UINT32_MAX - 5000; Banco b; b.cerrar(); b.desbloquear();
    Falso::ahora += 9999; b.c.actualizar(); COMPROBAR(b.movimientos == 1);
    ++Falso::ahora; b.c.actualizar(); COMPROBAR(b.movimientos == 2);
    Falso::ahora += 20000; b.c.actualizar(); COMPROBAR(!b.c.sensorVigente());
  });
  s.caso("servo no conecta al arrancar y valida posiciones", [] {
    ServoCerrojo servo(13, 700); COMPROBAR(servo.begin());
    COMPROBAR(Falso::escrituras == 0); COMPROBAR(!servo.moverA(45));
    Falso::falloAttach = true; COMPROBAR(!servo.bloquear() && Falso::escrituras == 0);
    Falso::falloAttach = false; COMPROBAR(servo.bloquear() && Falso::ultimoAngulo == 0);
    Falso::ahora = 700; servo.actualizar(); Falso::falloAttach = true;
    COMPROBAR(!servo.desbloquear()); ServoCerrojo invalido(35, 700);
    COMPROBAR(!invalido.begin() && !invalido.desbloquear());
  });
  s.caso("detach seguro al desbordar millis", [] {
    ServoCerrojo servo(13, 700); servo.begin(); Falso::ahora = UINT32_MAX - 300;
    COMPROBAR(servo.desbloquear()); Falso::ahora += 699; servo.actualizar();
    Falso::falloAttach = true; COMPROBAR(servo.bloquear()); // aun conectado
    Falso::ahora += 700; servo.actualizar(); COMPROBAR(!servo.bloquear());
  });
  s.caso("parser incremental descarta exceso y valida entero completo", [] {
    MonitorSerie m;
    for (char c : std::string("  ESTADO  ")) COMPROBAR(m.recibir(c) == MonitorSerie::Resultado::INCOMPLETA);
    COMPROBAR(m.recibir('\r') == MonitorSerie::Resultado::LISTA);
    COMPROBAR(std::strcmp(m.linea(), "estado") == 0);
    COMPROBAR(m.recibir('\n') == MonitorSerie::Resultado::INCOMPLETA);
    for (int i = 0; i < 100; ++i) m.recibir('a');
    COMPROBAR(m.recibir('\n') == MonitorSerie::Resultado::INVALIDA);
    m.recibir('\0'); m.recibir('x'); COMPROBAR(m.recibir('\n') == MonitorSerie::Resultado::INVALIDA);
    uint32_t valor = 0; COMPROBAR(MonitorSerie::entero("120", valor) && valor == 120);
    COMPROBAR(!MonitorSerie::entero("10x", valor)); COMPROBAR(!MonitorSerie::entero("-5", valor));
    COMPROBAR(!MonitorSerie::entero("", valor)); COMPROBAR(!MonitorSerie::entero("4294967296", valor));
    COMPROBAR(MonitorSerie::entero("4294967295", valor) && valor == UINT32_MAX);
  });
  return s.resultado();
}
