#pragma once

#include "MFRC522.h"
#include <iostream>
#include <stdexcept>

void reiniciarFalsos();

#define COMPROBAR(condicion) \
  do { \
    if (!(condicion)) throw std::runtime_error(#condicion); \
  } while (false)

class Suite {
public:
  template <typename Prueba>
  void caso(const char* nombre, Prueba prueba) {
    reiniciarFalsos();
    try {
      prueba();
      ++aprobadas_;
      std::cout << "[OK] " << nombre << '\n';
    } catch (const std::exception& error) {
      ++fallidas_;
      std::cerr << "[ERROR] " << nombre << " - " << error.what() << '\n';
    }
  }

  int resultado() const {
    std::cout << aprobadas_ << " aprobadas, " << fallidas_ << " fallidas\n";
    return fallidas_ == 0 ? 0 : 1;
  }

private:
  unsigned aprobadas_ = 0;
  unsigned fallidas_ = 0;
};
