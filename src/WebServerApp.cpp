#include "WebServerApp.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"

namespace {
float roundedTemperature(float temperature) {
  return roundf(temperature * 10.0f) / 10.0f;
}

int roundedHumidity(float humidity) {
  return static_cast<int>(roundf(humidity));
}
}  // namespace

WebServerApp::WebServerApp(MeasurementReader reader, WifiReconnect reconnect)
    : server_(80),
      reader_(reader),
      reconnect_(reconnect),
      historyCount_(0),
      lastHistorySlot_(0),
      hasLastHistorySlot_(false),
      wifiLostAt_(0) {}

void WebServerApp::begin() {
  loadHistory();
  updateHistory();
  setupRoutes();
  server_.begin();
  Serial.println("HTTP server started");
}

void WebServerApp::handle() {
  updateHistory();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");

    if (wifiLostAt_ == 0) {
      wifiLostAt_ = millis();
    }

    if (!reconnect_()) {
      if (millis() - wifiLostAt_ > WIFI_RECOVERY_RESET_MS) {
        Serial.println("WiFi not recovered, reset");
        ESP.restart();
      }
      delay(2000);
      return;
    }

    wifiLostAt_ = 0;
  }

  server_.handleClient();
}

void WebServerApp::setupRoutes() {
  server_.on("/", HTTP_GET, [this]() {
    sendFile("/index.html", "text/html; charset=utf-8");
  });

  server_.on("/css/style.css", HTTP_GET, [this]() {
    sendFile("/css/style.css", "text/css");
  });

  server_.on("/style.css", HTTP_GET, [this]() {
    sendFile("/css/style.css", "text/css");
  });

  server_.on("/data.json", HTTP_GET, [this]() {
    const Measurement measurement = reader_();
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "application/json; charset=utf-8", buildWeatherJson(measurement));
  });

  server_.on("/history", HTTP_POST, [this]() {
    String response;

    if (importHistory(server_.arg("plain"), response)) {
      server_.send(200, "application/json; charset=utf-8", response);
      return;
    }

    server_.send(400, "application/json; charset=utf-8", response);
  });

  server_.on("/js/script.js", HTTP_GET, [this]() {
    const Measurement measurement = reader_();
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "application/javascript; charset=utf-8", buildScript(measurement));
  });

  server_.on("/script.js", HTTP_GET, [this]() {
    const Measurement measurement = reader_();
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "application/javascript; charset=utf-8", buildScript(measurement));
  });

  server_.onNotFound([this]() {
    sendFile("/index.html", "text/html; charset=utf-8");
  });
}

void WebServerApp::loadHistory() {
  historyCount_ = 0;
  hasLastHistorySlot_ = false;

  if (!LittleFS.exists(HISTORY_FILE)) {
    return;
  }

  File file = LittleFS.open(HISTORY_FILE, "r");
  if (!file) {
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error || !doc.is<JsonArray>()) {
    return;
  }

  JsonArray array = doc.as<JsonArray>();
  const size_t total = array.size();
  const size_t start = total > MAX_HISTORY_RECORDS ? total - MAX_HISTORY_RECORDS : 0;

  for (size_t i = start; i < total && historyCount_ < MAX_HISTORY_RECORDS; i++) {
    JsonObject item = array[i];
    HistoryRecord& record = history_[historyCount_++];
    record.slot = item["slot"] | 0;
    record.hour = String(item["hour"] | "");
    record.temperature = roundedTemperature(item["temperature"] | 0.0f);
    record.humidity = roundedHumidity(item["humidity"] | 0.0f);
    record.pressure = item["pressure"] | 0;
  }

  if (historyCount_ > 0) {
    lastHistorySlot_ = history_[historyCount_ - 1].slot;
    hasLastHistorySlot_ = true;
  }
}

bool WebServerApp::saveHistory() {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  for (size_t i = 0; i < historyCount_; i++) {
    JsonObject item = array.add<JsonObject>();
    item["slot"] = history_[i].slot;
    item["hour"] = history_[i].hour;
    item["temperature"] = history_[i].temperature;
    item["humidity"] = history_[i].humidity;
    item["pressure"] = history_[i].pressure;
  }

  File file = LittleFS.open(HISTORY_FILE, "w");
  if (!file) {
    Serial.println("Nie udalo sie zapisac historii");
    return false;
  }

  serializeJson(doc, file);
  file.close();
  return true;
}

bool WebServerApp::importHistory(const String& payload, String& response) {
  JsonDocument responseDoc;
  String jsonPayload = payload;
  jsonPayload.trim();

  if (jsonPayload.length() == 0) {
    responseDoc["error"] = "empty body";
    serializeJson(responseDoc, response);
    return false;
  }

  const char firstChar = jsonPayload.charAt(0);
  if (firstChar != '{' && firstChar != '[') {
    const int objectStart = jsonPayload.indexOf('{');
    const int arrayStart = jsonPayload.indexOf('[');
    const int start = objectStart < 0 ? arrayStart : (arrayStart < 0 ? objectStart : min(objectStart, arrayStart));
    const int objectEnd = jsonPayload.lastIndexOf('}');
    const int arrayEnd = jsonPayload.lastIndexOf(']');
    const int end = max(objectEnd, arrayEnd);

    if (start >= 0 && end > start) {
      jsonPayload = jsonPayload.substring(start, end + 1);
    }
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonPayload);

  if (error) {
    responseDoc["error"] = "invalid json";
    responseDoc["detail"] = error.c_str();
    responseDoc["bytes"] = payload.length();
    serializeJson(responseDoc, response);
    return false;
  }

  JsonArray array;
  if (doc.is<JsonArray>()) {
    array = doc.as<JsonArray>();
  } else if (doc["measurements"].is<JsonArray>()) {
    array = doc["measurements"].as<JsonArray>();
  }

  if (array.isNull()) {
    responseDoc["error"] = "expected array or measurements array";
    serializeJson(responseDoc, response);
    return false;
  }

  size_t validCount = 0;
  for (JsonObject item : array) {
    if (item["slot"].isNull() || item["hour"].isNull() || item["temperature"].isNull()
        || item["humidity"].isNull() || item["pressure"].isNull()) {
      continue;
    }

    validCount++;
  }

  if (validCount == 0) {
    responseDoc["error"] = "no valid history records";
    serializeJson(responseDoc, response);
    return false;
  }

  historyCount_ = 0;
  for (JsonObject item : array) {
    if (item["slot"].isNull() || item["hour"].isNull() || item["temperature"].isNull()
        || item["humidity"].isNull() || item["pressure"].isNull()) {
      continue;
    }

    if (historyCount_ == MAX_HISTORY_RECORDS) {
      for (size_t i = 1; i < historyCount_; i++) {
        history_[i - 1] = history_[i];
      }
      historyCount_--;
    }

    HistoryRecord& record = history_[historyCount_++];
    record.slot = item["slot"] | 0;
    record.hour = String(item["hour"] | "");
    record.temperature = roundedTemperature(item["temperature"] | 0.0f);
    record.humidity = roundedHumidity(item["humidity"] | 0.0f);
    record.pressure = item["pressure"] | 0;
  }

  lastHistorySlot_ = history_[historyCount_ - 1].slot;
  hasLastHistorySlot_ = true;

  if (!saveHistory()) {
    responseDoc["error"] = "history save failed";
    serializeJson(responseDoc, response);
    return false;
  }

  responseDoc["status"] = "ok";
  responseDoc["imported"] = historyCount_;
  serializeJson(responseDoc, response);
  return true;
}

void WebServerApp::updateHistory() {
  const time_t now = time(nullptr);
  if (now < 100000 || !isFullHour(now)) {
    return;
  }

  const uint32_t currentSlot = measurementSlot(now);
  if (hasLastHistorySlot_ && lastHistorySlot_ == currentSlot) {
    return;
  }

  const Measurement measurement = reader_();
  addHistoryRecord(measurement, now);
  lastHistorySlot_ = currentSlot;
  hasLastHistorySlot_ = true;
}

void WebServerApp::addHistoryRecord(const Measurement& measurement, time_t now) {
  if (historyCount_ == MAX_HISTORY_RECORDS) {
    for (size_t i = 1; i < historyCount_; i++) {
      history_[i - 1] = history_[i];
    }
    historyCount_--;
  }

  HistoryRecord& record = history_[historyCount_++];
  record.slot = measurementSlot(now);
  record.hour = measurementHour(now);
  record.temperature = roundedTemperature(measurement.temperature);
  record.humidity = roundedHumidity(measurement.humidity);
  record.pressure = static_cast<int>(roundf(measurement.pressure));

  saveHistory();
}

String WebServerApp::buildWeatherJson(const Measurement& current) {
  JsonDocument doc;
  JsonObject currentJson = doc["current"].to<JsonObject>();
  currentJson["temperature"] = roundedTemperature(current.temperature);
  currentJson["humidity"] = roundedHumidity(current.humidity);
  currentJson["pressure"] = static_cast<int>(roundf(current.pressure));
  currentJson["batteryVoltage"] = roundf(current.batteryVoltage * 100.0f) / 100.0f;
  currentJson["batteryPercent"] = current.batteryPercent;

  JsonArray measurements = doc["measurements"].to<JsonArray>();
  for (size_t i = 0; i < historyCount_; i++) {
    JsonObject item = measurements.add<JsonObject>();
    item["slot"] = history_[i].slot;
    item["hour"] = history_[i].hour;
    item["temperature"] = history_[i].temperature;
    item["humidity"] = history_[i].humidity;
    item["pressure"] = history_[i].pressure;
  }

  String output;
  serializeJson(doc, output);
  return output;
}

String WebServerApp::buildScript(const Measurement& current) {
  String script = loadTextFile("/js/script.js");
  script.replace("{{WEATHER_DATA}}", buildWeatherJson(current));
  return script;
}

String WebServerApp::loadTextFile(const char* path) {
  if (!LittleFS.exists(path)) {
    return String();
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return String();
  }

  String content = file.readString();
  file.close();
  return content;
}

void WebServerApp::sendFile(const char* path, const char* contentType) {
  if (!LittleFS.exists(path)) {
    Serial.print("Brak pliku LittleFS: ");
    Serial.println(path);
    server_.send(404, "text/plain", "Not found");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    server_.send(404, "text/plain", "Not found");
    return;
  }

  server_.sendHeader("Cache-Control", "no-cache");
  server_.streamFile(file, contentType);
  file.close();
}

bool WebServerApp::isFullHour(time_t now) const {
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return timeinfo.tm_min == 0;
}

uint32_t WebServerApp::measurementSlot(time_t now) const {
  return static_cast<uint32_t>(now / HISTORY_INTERVAL_SECONDS);
}

String WebServerApp::measurementHour(time_t now) const {
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02d.%02d %02d:00", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour);
  return String(buffer);
}
