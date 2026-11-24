#pragma once

#include "LocalLink.h"

class SerialLink : public LocalLink {
public:
  bool begin() override;
  bool send(const Protocol::Message& message) override;
  void onReceive(std::function<void(const Protocol::Message&)> callback) override;

private:
  std::function<void(const Protocol::Message&)> callback_;
};
