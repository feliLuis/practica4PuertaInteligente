#include "CredentialList.h"

#include <string.h>

CredentialList::CredentialList() : credentials_{}, count_(0) {}

bool CredentialList::matches(const Credential& credential, const uint8_t* uid, uint8_t length) {
  return credential.length == length && memcmp(credential.uid, uid, length) == 0;
}

bool CredentialList::add(const uint8_t* uid, uint8_t length, Action action,
                         const char* credentialId) {
  // RFID credentials can authorize only unlock actions.
  if (uid == nullptr || (length != 4 && length != 7 && length != 10) ||
      credentialId == nullptr || credentialId[0] == '\0' ||
      strlen(credentialId) >= sizeof(credentials_[0].credentialId) ||
      action != Action::UNLOCK) return false;

  uint8_t index = 0;
  while (index < count_ && !matches(credentials_[index], uid, length)) ++index;
  if (index == count_) {
    if (count_ == MAX_CREDENTIALS) return false;
    ++count_;
  }
  Credential& credential = credentials_[index];
  credential.length = length;
  credential.action = action;
  memcpy(credential.uid, uid, length);
  strcpy(credential.credentialId, credentialId);
  return true;
}

bool CredentialList::find(const uint8_t* uid, uint8_t length, Action& action,
                          const char*& credentialId) const {
  credentialId = nullptr;
  if (uid == nullptr || (length != 4 && length != 7 && length != 10)) return false;
  for (uint8_t i = 0; i < count_; ++i) {
    if (matches(credentials_[i], uid, length)) {
      action = credentials_[i].action;
      credentialId = credentials_[i].credentialId;
      return true;
    }
  }
  return false;
}
