#pragma once

#include <Arduino.h>
#include <Wire.h>

class SHT41 {
public:
  explicit SHT41(TwoWire& wire, uint8_t address = 0x44);
  bool read(float& temperature, float& humidity);

private:
  TwoWire& wire_;
  uint8_t address_;
};
