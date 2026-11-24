#pragma once

#include "Protocol.h"

#include <functional>

class LocalLink {
public:
  virtual ~LocalLink() = default;
  virtual bool begin() = 0;
  virtual bool send(const Protocol::Message& message) = 0;
  virtual void onReceive(std::function<void(const Protocol::Message&)> callback) = 0;
};
