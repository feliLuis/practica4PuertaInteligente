#pragma once

#include "config.h"
#include <Protocol.h>
#include <stdint.h>

class DoorSensor {
public:
  DoorSensor();
  void begin();
  bool update();
  Protocol::DoorState state() const;

private:
  Protocol::DoorState stable_;
  Protocol::DoorState candidate_;
  uint32_t stableSince_;
};
