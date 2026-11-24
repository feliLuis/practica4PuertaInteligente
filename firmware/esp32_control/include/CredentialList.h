#pragma once

#include "Types.h"
#include <stdint.h>

class CredentialList {
public:
  CredentialList();
  bool add(const uint8_t* uid, uint8_t length, Action action, const char* credentialId);
  bool buscar(const uint8_t* uid, uint8_t length, Action& action,
              const char*& credentialId) const;

private:
  struct Credential {
    uint8_t uid[10];
    uint8_t length;
    Action action;
    char credentialId[24];
  };
  static constexpr uint8_t MAX_CREDENTIALS = 8;
  Credential credentials_[MAX_CREDENTIALS];
  uint8_t count_;
  static bool coinciden(const Credential& credential, const uint8_t* uid, uint8_t length);
};
