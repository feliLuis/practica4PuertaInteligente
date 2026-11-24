#pragma once
#include "Arduino.h"
#include <iostream>
#include <stdexcept>
#include <string>

#define COMPROBAR(expresion) do { if (!(expresion)) \
  throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                           " " + #expresion); } while (false)

class Suite {
public:
  template<class Funcion> void caso(const char* nombre, Funcion funcion) {
    Falso::reiniciar();
    try { funcion(); ++aprobadas_; std::cout << "OK " << nombre << "\n"; }
    catch (const std::exception& error) {
      ++fallidas_; std::cerr << "FALLO " << nombre << ": " << error.what() << "\n";
    }
  }
  int resultado() const {
    std::cout << aprobadas_ << " aprobadas, " << fallidas_ << " fallidas\n";
    return fallidas_ == 0 ? 0 : 1;
  }
private:
  int aprobadas_ = 0;
  int fallidas_ = 0;
};
