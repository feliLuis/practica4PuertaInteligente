#pragma once
// Sustitutos de prueba: no acceden a GPIO, puertos USB ni dispositivos.
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <deque>

using byte = uint8_t;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT_PULLUP = 2;
namespace Falso {
extern uint32_t ahora;
extern int nivel[40];
extern int modo[40];
extern bool falloAttach;
extern int escrituras;
extern int ultimoAngulo;
extern byte versionRfid;
extern bool tarjetaPresente;
extern bool lecturaRfidOk;
extern byte uid[10];
extern byte uidLen;
extern int inicializacionesRfid;
extern int spi[4];
void reiniciar();
}
inline uint32_t millis() { return Falso::ahora; }
inline int digitalRead(uint8_t pin) { return Falso::nivel[pin]; }
inline void pinMode(uint8_t pin, int modo) { Falso::modo[pin] = modo; }

struct SerieFalsa {
  std::string salida;
  std::deque<char> entrada;
  void begin(uint32_t) {}
  int available() const { return static_cast<int>(entrada.size()); }
  int read() { const char c = entrada.front(); entrada.pop_front(); return c; }
  void println(const char* texto) { salida += texto; salida += '\n'; }
  void printf(const char* formato, ...) {
    char buffer[1024];
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(buffer, sizeof(buffer), formato, argumentos);
    va_end(argumentos);
    salida += buffer;
  }
};
extern SerieFalsa Serial;
