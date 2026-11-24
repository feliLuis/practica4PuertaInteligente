#pragma once

#include <cstdint>
#include <string>

using byte = uint8_t;
constexpr byte LOW = 0;
constexpr byte HIGH = 1;
constexpr byte INPUT_PULLUP = 2;

namespace Falso {
extern uint32_t ahora;
extern byte nivel[40];
extern byte modo[40];
}

inline uint32_t millis() { return Falso::ahora; }
inline int digitalRead(byte pin) { return Falso::nivel[pin]; }
inline void pinMode(byte pin, byte modo) { Falso::modo[pin] = modo; }

struct SerieFalsa {
  std::string salida;
  void println(const char* mensaje);
  void printf(const char* formato, ...);
};
extern SerieFalsa Serial;
