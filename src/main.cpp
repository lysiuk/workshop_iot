#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include "secrets.h"

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT11;
constexpr uint32_t READ_INTERVAL_MS = 10000;
constexpr uint32_t MIN_DHT_INTERVAL_MS = 2000;
constexpr size_t HISTORY_SIZE = 10;

struct Measurement {
  float temperature;
  float humidity;
  uint32_t timestamp;
};

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
Measurement history[HISTORY_SIZE];
size_t historyIndex = 0;
size_t historyCount = 0;
uint32_t lastAttemptMs = 0;
bool attempted = false;
bool sensorOk = false;
uint32_t lastSuccessMs = 0;

void addMeasurement(float temp, float hum) {
  history[historyIndex] = {temp, hum, millis()};
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) ++historyCount;
}

bool readSensor(bool force = false) {
  const uint32_t now = millis();

  if (attempted && (uint32_t)(now - lastAttemptMs) < (force ? MIN_DHT_INTERVAL_MS : READ_INTERVAL_MS)) {
    return false;
  }

  lastAttemptMs = now;
  attempted = true;
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();

  if (isnan(hum) || isnan(temp)) {
    sensorOk = false;
    Serial.println("[DHT22] Read failed; retaining prior history");
    return false;
  }

  sensorOk = true;
  lastSuccessMs = now;
  addMeasurement(temp, hum);
  Serial.printf("[DHT22] Temperature: %.1f C | Humidity: %.1f %%\n", temp, hum);

  return true;
}

void noCache() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Access-Control-Allow-Origin", "*");
}

void sendCurrent() {
  noCache();

  if (historyCount == 0) {
    server.send(
      200,
      "application/json", "{\"valid\":false,\"temperature\":null,\"humidity\":null,\"timestamp\":null,\"sensorOk\":false,\"intervalMs\":10000}"
    );
    return;
  }

  const Measurement &m = history[(historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE];
  char json[220];
  snprintf(
    json,
    sizeof(json),
    "{\"valid\":true,\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%lu,\"sensorOk\":%s,\"intervalMs\":%lu}",
    m.temperature,
    m.humidity,
    (unsigned long)m.timestamp,
    sensorOk ? "true" : "false",
    (unsigned long)READ_INTERVAL_MS
  );
  server.send(200, "application/json", json);
}

void sendHistory() {
  noCache();
  String json;
  json.reserve(HISTORY_SIZE * 85 + 2);
  json = "[";

  for (size_t i = 0; i < historyCount; ++i) {
    const size_t index = (historyIndex + HISTORY_SIZE - historyCount + i) % HISTORY_SIZE;
    const Measurement &m = history[index];

    if (i != 0) json += ',';

    char entry[105];
    snprintf(
      entry,
      sizeof(entry),
      "{\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%lu}",
      m.temperature,
      m.humidity,
      (unsigned long)m.timestamp
    );
    json += entry;
  }

  json += "]";
  server.send(200, "application/json", json);
}

void sendFile(const char *path, const char *type) {
  File file = LittleFS.open(path, "r");

  if (!file) {
    server.send(404, "text/plain", "File not found. Run Upload Filesystem Image.");
    return;
  }

  server.streamFile(file, type);
  file.close();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  dht.begin();

  // if (!LittleFS.begin(true)) {
  //   Serial.println("[ERROR] LittleFS mount failed");
  // }

  bool fsReady = LittleFS.begin(false);
  Serial.printf("[LittleFS] Mounted: %s\n", fsReady ? "YES" : "NO");

  if (fsReady) {
    Serial.printf("[LittleFS] Total: %u bytes\n", (unsigned)LittleFS.totalBytes());
    Serial.printf("[LittleFS] Used: %u bytes\n", (unsigned)LittleFS.usedBytes());

    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {
      Serial.printf("[FILE] %s (%u bytes)\n", file.name(), (unsigned)file.size());
      file = root.openNextFile();
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  const uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - start) < 20000) {
    delay(400);
    Serial.print('.');
  }

  Serial.printf("index.html: %s\n", LittleFS.exists("/index.html") ? "FOUND" : "NOT FOUND");
  Serial.printf("style.css: %s\n", LittleFS.exists("/style.css") ? "FOUND" : "NOT FOUND");
  Serial.printf("app.js: %s\n", LittleFS.exists("/app.js") ? "FOUND" : "NOT FOUND");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Open http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] No connection. Check credentials and 2.4 GHz network.");
  }

  server.on("/", HTTP_GET, []() { sendFile("/index.html", "text/html; charset=utf-8"); });
  server.on("/style.css", HTTP_GET, []() { sendFile("/style.css", "text/css; charset=utf-8"); });
  server.on("/app.js", HTTP_GET, []() { sendFile("/app.js", "application/javascript; charset=utf-8"); });
  server.on("/api/current", HTTP_GET, sendCurrent);
  server.on("/api/history", HTTP_GET, sendHistory);
  server.on("/api/refresh", HTTP_POST, []() {
    readSensor(true);
    sendCurrent();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "404 Not found"); });
  server.begin();
  Serial.println("[HTTP] Server started");
  readSensor();
}

void loop() {
  server.handleClient();

  if (!attempted || (uint32_t)(millis() - lastAttemptMs) >= READ_INTERVAL_MS) {
    readSensor();
  }

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastReconnect = 0;

    if ((uint32_t)(millis() - lastReconnect) >= 10000) {
      lastReconnect = millis();
      WiFi.reconnect();
    }
  }

  delay(2);
}
