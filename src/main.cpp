#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "LPS22HH.h"
#include "SHT41.h"
#include "WebServerApp.h"
#include "config.h"

SHT41 sht(Wire);
LPS22HH lps(Wire);
WebServerApp* webServer = nullptr;
float lastValidPressure = NAN;

bool isRealisticPressure(float pressure) {
  return pressure >= PRESSURE_MIN_HPA && pressure <= PRESSURE_MAX_HPA;
}

float reducePressure(float pressure) {
  return pressure + PRESSURE_REDUCTION_HPA;
}

float readBatteryVoltage() {
  const int raw = analogRead(BATTERY_ADC_PIN);
  return static_cast<float>(raw) * 3.3f / 4095.0f * BATTERY_VOLTAGE_DIVIDER;
}

int batteryPercentFromVoltage(float voltage) {
  const float percent = (voltage - BATTERY_EMPTY_VOLTAGE) * 100.0f
      / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE);
  return constrain(static_cast<int>(roundf(percent)), 0, 100);
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    delay(1000);
  }

  Serial.println("Laczenie z WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    esp_task_wdt_reset();
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("Nie udalo sie polaczyc z WiFi");
  return false;
}

void syncTime() {
  const char* ntpServers[] = {
      "pool.ntp.org",
      "0.pool.ntp.org",
      "1.pool.ntp.org",
      "time.google.com",
      "time.cloudflare.com",
  };

  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", ntpServers[0], ntpServers[1], ntpServers[2]);

  while (time(nullptr) < 100000) {
    for (const char* server : ntpServers) {
      esp_task_wdt_reset();
      configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", server);

      for (int i = 0; i < 10; i++) {
        if (time(nullptr) >= 100000) {
          Serial.print("Czas OK: ");
          Serial.println(server);
          return;
        }

        esp_task_wdt_reset();
        delay(500);
      }

      Serial.print("Nie udalo sie ustawic czasu z ");
      Serial.println(server);
    }
  }
}

Measurement readValues() {
  float tempSht = NAN;
  float humidity = NAN;
  if (!sht.read(tempSht, humidity)) {
    Serial.println("Blad odczytu SHT41");
    tempSht = 0.0f;
    humidity = 0.0f;
  }

  float tempLps = tempSht;
  float pressure = NAN;

  for (int attempt = 0; attempt < PRESSURE_READ_ATTEMPTS; attempt++) {
    float readPressure = NAN;
    float readTempLps = tempLps;

    if (lps.read(readPressure, readTempLps)) {
      tempLps = readTempLps;

      if (isRealisticPressure(readPressure)) {
        pressure = readPressure;
        lastValidPressure = readPressure;
        break;
      }

      Serial.print("Nierealistyczne cisnienie: ");
      Serial.print(readPressure);
      Serial.print(" hPa, proba ");
      Serial.println(attempt + 1);
    } else {
      Serial.print("Blad odczytu LPS22HH, proba ");
      Serial.println(attempt + 1);
    }

    delay(50);
  }

  if (isnan(pressure)) {
    if (!isnan(lastValidPressure)) {
      pressure = lastValidPressure;
    } else {
      pressure = PRESSURE_FALLBACK_HPA;
    }

    Serial.print("Uzywam zastepczego cisnienia: ");
    Serial.print(pressure);
    Serial.println(" hPa");
  }

  Measurement measurement;
  measurement.temperature = tempSht;
  measurement.humidity = humidity;
  measurement.pressure = reducePressure(pressure);
  measurement.batteryVoltage = readBatteryVoltage();
  measurement.batteryPercent = batteryPercentFromVoltage(measurement.batteryVoltage);
  return measurement;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  esp_task_wdt_init(60, true);
  esp_task_wdt_add(nullptr);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);

  if (!LittleFS.begin(true)) {
    Serial.println("Nie udalo sie uruchomic LittleFS");
  }

  if (!lps.begin()) {
    Serial.println("Nie udalo sie zainicjalizowac LPS22HH");
  }

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  pinMode(BATTERY_ADC_PIN, INPUT);

  if (!connectWifi()) {
    Serial.println("Brak WiFi, restart...");
    delay(5000);
    ESP.restart();
  }

  syncTime();

  static WebServerApp server(readValues, connectWifi);
  webServer = &server;
  webServer->begin();
}

void loop() {
  esp_task_wdt_reset();

  if (webServer != nullptr) {
    webServer->handle();
  }

  delay(10);
}
