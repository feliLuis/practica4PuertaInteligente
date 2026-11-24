#pragma once
#include "Arduino.h"
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expression) do { if (!(expression)) \
  throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                           " " + #expression); } while (false)

class Suite {
public:
  template<class Funcion> void testCase(const char* name, Funcion test) {
    Fake::reset();
    try { test(); ++passed_; std::cout << "OK " << name << "\n"; }
    catch (const std::exception& error) {
      ++failed_; std::cerr << "FAIL " << name << ": " << error.what() << "\n";
    }
  }
  int result() const {
    std::cout << passed_ << " passed, " << failed_ << " failed\n";
    return failed_ == 0 ? 0 : 1;
  }
private:
  int passed_ = 0;
  int failed_ = 0;
};
