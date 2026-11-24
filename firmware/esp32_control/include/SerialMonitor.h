#pragma once

#include "config.h"
#include <ctype.h>
#include <stdint.h>
#include <string.h>

// Parser avoids String, blocking waits, and dynamic allocation.
class SerialMonitor {
public:
  enum class Result { INCOMPLETE, READY, INVALID };

  Result receive(char character) {
    if (character == '\r' || character == '\n') {
      if (discarding_) {
        length_ = 0;
        discarding_ = false;
        return Result::INVALID;
      }
      while (length_ > 0 && buffer_[length_ - 1] == ' ') --length_;
      buffer_[length_] = '\0';
      length_ = 0;
      return buffer_[0] == '\0' ? Result::INCOMPLETE : Result::READY;
    }
    const auto byte = static_cast<unsigned char>(character);
    if (discarding_) return Result::INCOMPLETE;
    if (byte < 32 || byte > 126 || length_ >= sizeof(buffer_) - 1) {
      discarding_ = true; // Never execute the suffix of a truncated line.
      return Result::INCOMPLETE;
    }
    if (length_ == 0 && character == ' ') return Result::INCOMPLETE;
    buffer_[length_++] = static_cast<char>(tolower(byte));
    return Result::INCOMPLETE;
  }

  const char* line() const { return buffer_; }

  static bool parseUnsigned(const char* text, uint32_t& value) {
    if (text == nullptr || *text == '\0') return false;
    uint32_t numero = 0;
    for (const char* p = text; *p != '\0'; ++p) {
      if (*p < '0' || *p > '9') return false;
      const uint32_t digit = static_cast<uint32_t>(*p - '0');
      if (numero > (UINT32_MAX - digit) / 10) return false;
      numero = numero * 10 + digit;
    }
    value = numero;
    return true;
  }

private:
  char buffer_[Config::COMMAND_CAPACITY]{};
  uint8_t length_ = 0;
  bool discarding_ = false;
};
