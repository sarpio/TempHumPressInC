#include "SHT41.h"

SHT41::SHT41(TwoWire& wire, uint8_t address) : wire_(wire), address_(address) {}

bool SHT41::read(float& temperature, float& humidity) {
  wire_.beginTransmission(address_);
  wire_.write(0xFD);
  if (wire_.endTransmission() != 0) {
    return false;
  }

  delay(10);

  if (wire_.requestFrom(address_, static_cast<uint8_t>(6)) != 6) {
    return false;
  }

  uint8_t data[6];
  for (uint8_t i = 0; i < sizeof(data); i++) {
    data[i] = wire_.read();
  }

  const uint16_t rawTemp = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t rawHum = (static_cast<uint16_t>(data[3]) << 8) | data[4];

  temperature = -45.0f + (175.0f * static_cast<float>(rawTemp) / 65535.0f);
  humidity = -6.0f + (125.0f * static_cast<float>(rawHum) / 65535.0f);
  humidity = constrain(humidity, 0.0f, 100.0f);

  return true;
}
