#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"

struct Measurement {
  float temperature;
  float humidity;
  float pressure;
  int batteryPercent;
};

using MeasurementReader = Measurement (*)();
using WifiReconnect = bool (*)();

class WebServerApp {
public:
  WebServerApp(MeasurementReader reader, WifiReconnect reconnect);
  void begin();
  void handle();

private:
  struct HistoryRecord {
    uint32_t slot;
    String hour;
    float temperature;
    float humidity;
    int pressure;
  };

  void setupRoutes();
  void loadHistory();
  void saveHistory();
  void updateHistory();
  void addHistoryRecord(const Measurement& measurement, time_t now);
  String buildWeatherJson(const Measurement& current);
  String buildScript(const Measurement& current);
  String loadTextFile(const char* path);
  void sendFile(const char* path, const char* contentType);
  bool isFullHour(time_t now) const;
  uint32_t measurementSlot(time_t now) const;
  String measurementHour(time_t now) const;

  WebServer server_;
  MeasurementReader reader_;
  WifiReconnect reconnect_;
  HistoryRecord history_[MAX_HISTORY_RECORDS];
  size_t historyCount_;
  uint32_t lastHistorySlot_;
  bool hasLastHistorySlot_;
  uint32_t wifiLostAt_;
};
