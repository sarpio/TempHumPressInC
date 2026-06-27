#include "LPS22HH.h"

LPS22HH::LPS22HH(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

bool LPS22HH::begin() {
  return writeRegister(0x10, 0x02) && writeRegister(0x11, 0x10);
}

uint8_t LPS22HH::whoami() {
  uint8_t value = 0;
  readRegister(0x0F, value);
  return value;
}

bool LPS22HH::read(float& pressure, float& temperature) {
  uint8_t ctrl2 = 0;
  if (!readRegister(0x11, ctrl2)) {
    return false;
  }

  if (!writeRegister(0x11, ctrl2 | 0x01)) {
    return false;
  }

  bool ready = false;
  for (uint8_t i = 0; i < 50; i++) {
    uint8_t status = 0;
    if (!readRegister(0x27, status)) {
      return false;
    }

    if ((status & 0x03) == 0x03) {
      ready = true;
      break;
    }

    delay(10);
  }

  if (!ready) {
    return false;
  }

  uint8_t data[5];
  if (!readRegisters(0x28 | 0x80, data, sizeof(data))) {
    return false;
  }

  int32_t rawPressure = static_cast<int32_t>(data[0])
      | (static_cast<int32_t>(data[1]) << 8)
      | (static_cast<int32_t>(data[2]) << 16);

  if (rawPressure & 0x800000) {
    rawPressure -= 0x1000000;
  }

  int16_t rawTemp = static_cast<int16_t>(
      static_cast<uint16_t>(data[3]) | (static_cast<uint16_t>(data[4]) << 8));

  pressure = static_cast<float>(rawPressure) / 4096.0f;
  temperature = static_cast<float>(rawTemp) / 100.0f;

  return true;
}

bool LPS22HH::writeRegister(uint8_t reg, uint8_t value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool LPS22HH::readRegister(uint8_t reg, uint8_t& value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0) {
    return false;
  }

  if (wire_.requestFrom(address_, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = wire_.read();
  return true;
}

bool LPS22HH::readRegisters(uint8_t reg, uint8_t* buffer, size_t length) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0) {
    return false;
  }

  if (wire_.requestFrom(address_, static_cast<uint8_t>(length)) != length) {
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    buffer[i] = wire_.read();
  }

  return true;
}
