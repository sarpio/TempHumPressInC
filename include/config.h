#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr const char* WIFI_SSID = "PLAY_Swiatlowodowy_B402";
constexpr const char* WIFI_PASSWORD = "9x6gEw#81S95";

constexpr int I2C_SDA_PIN = 5;
constexpr int I2C_SCL_PIN = 6;
constexpr uint32_t I2C_FREQUENCY = 400000;

constexpr int BATTERY_ADC_PIN = 1;
constexpr float BATTERY_VOLTAGE_DIVIDER = 2.156f;
constexpr float BATTERY_EMPTY_VOLTAGE = 3.3f;
constexpr float BATTERY_FULL_VOLTAGE = 4.2f;

constexpr float PRESSURE_MIN_HPA = 850.0f;
constexpr float PRESSURE_MAX_HPA = 1100.0f;
constexpr float PRESSURE_REDUCTION_HPA = 0.0f;
constexpr float PRESSURE_FALLBACK_HPA = 1013.25f;
constexpr int PRESSURE_READ_ATTEMPTS = 3;

constexpr uint32_t HISTORY_INTERVAL_SECONDS = 60 * 60;
constexpr size_t MAX_HISTORY_RECORDS = 7 * 24;
constexpr const char* HISTORY_FILE = "/measurements_history.json";

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RECOVERY_RESET_MS = 120000;
