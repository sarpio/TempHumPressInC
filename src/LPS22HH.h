#pragma once

#include <Arduino.h>
#include <Wire.h>

class LPS22HH {
public:
  explicit LPS22HH(TwoWire& wire, uint8_t address = 0x5D);
  bool begin();
  uint8_t whoami();
  bool read(float& pressure, float& temperature);

private:
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegister(uint8_t reg, uint8_t& value);
  bool readRegisters(uint8_t reg, uint8_t* buffer, size_t length);

  TwoWire& wire_;
  uint8_t address_;
};
