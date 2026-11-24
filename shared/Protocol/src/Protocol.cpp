#include "Protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace Protocol {

const char* name(DoorState state) {
  switch (state) {
    case DoorState::OPEN: return "abierta";
    case DoorState::CLOSED: return "cerrada";
    default: return "desconocida";
  }
}

const char* name(Type type) {
  switch (type) {
    case Type::REED_STATE: return "REED_ESTADO";
    case Type::RFID_PRESENTED: return "RFID_PRESENTADA";
    case Type::HEARTBEAT: return "LATIDO";
    default: return "DESCONOCIDO";
  }
}

bool uidToHex(const uint8_t* uid, uint8_t length, char* output, size_t capacity) {
  if (uid == nullptr || output == nullptr || length > 10) return false;
  if (capacity < static_cast<size_t>(length * 2 + 1)) return false;

  char* cursor = output;
  for (uint8_t i = 0; i < length; ++i) {
    snprintf(cursor, 3, "%02X", uid[i]);
    cursor += 2;
  }
  *cursor = '\0';
  return true;
}

static int hexValue(char character) {
  if (character >= '0' && character <= '9') return character - '0';
  character = static_cast<char>(toupper(static_cast<unsigned char>(character)));
  if (character >= 'A' && character <= 'F') return character - 'A' + 10;
  return -1;
}

bool hexToUid(const char* text, uint8_t* uid, uint8_t capacity, uint8_t& length) {
  length = 0;
  if (text == nullptr || uid == nullptr) return false;

  char limpio[32];
  size_t n = 0;
  for (const char* p = text; *p != '\0'; ++p) {
    if (isspace(static_cast<unsigned char>(*p))) continue;
    if (n >= sizeof(limpio) - 1) return false;
    limpio[n++] = *p;
  }
  limpio[n] = '\0';

  if (n == 0 || (n & 1) != 0) return false;
  const uint8_t count = static_cast<uint8_t>(n / 2);
  if (count > capacity) return false;

  for (uint8_t i = 0; i < count; ++i) {
    int alto = hexValue(limpio[i * 2]);
    int bajo = hexValue(limpio[i * 2 + 1]);
    if (alto < 0 || bajo < 0) return false;
    uid[i] = static_cast<uint8_t>((alto << 4) | bajo);
  }
  length = count;
  return true;
}

} // namespace Protocol
