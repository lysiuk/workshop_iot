#include <Arduino.h>
#include <DHT.h>
#include <math.h>

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT11;
constexpr uint8_t BUFFER_SIZE = 12;
constexpr uint32_t READ_INTERVAL = 5000;
constexpr uint32_t RETRY_INTERVAL = 2100;
constexpr uint8_t MAX_ATTEMPTS = 3;

constexpr float MIN_TEMP = 0.0f;
constexpr float MAX_TEMP = 50.0f;
constexpr float MIN_HUM = 20.0f;
constexpr float MAX_HUM = 90.0f;

DHT dht(DHT_PIN, DHT_TYPE);

struct Measurement {
  float temperature = NAN;
  float humidity = NAN;
  bool valid = false;
};

struct Statistics {
  float average = 0;
  float min = 0;
  float max = 0;
  float range = 0;
};

Measurement readings[BUFFER_SIZE];

uint8_t bufferIndex = 0;
uint8_t samplesInWindow = 0;

uint32_t lastWindowStart = 0;
uint32_t currentSlotStart = 0;
uint32_t lastAttemptTime = 0;
uint32_t totalReadErrors = 0;
uint32_t totalFailedSamples = 0;
uint32_t totalRetries = 0;
uint32_t totalSamples = 0;

uint8_t currentAttempt = 0;
bool readingInProgress = false;

// ========== VALIDATION ==========
bool isValid(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity)) {
    return false;
  }

  if (!isfinite(temperature) || !isfinite(humidity)) {
    return false;
  }

  return temperature >= MIN_TEMP &&
         temperature <= MAX_TEMP &&
         humidity >= MIN_HUM &&
         humidity <= MAX_HUM;
}

// ========== STATISTICS ==========
Statistics calculateStats(const float values[], uint8_t count) {
  Statistics stats;

  if (count == 0) {
    return stats;
  }

  float sum = 0;
  stats.min = values[0];
  stats.max = values[0];

  for (uint8_t i = 0; i < count; i++) {
    float value = values[i];

    sum += value;

    if (value < stats.min) stats.min = value;
    if (value > stats.max) stats.max = value;
  }

  stats.average = sum / count;
  stats.range = stats.max - stats.min;

  return stats;
}

// ========== DISPLAY ==========
void printProgress() {
  const uint8_t width = 12;

  Serial.print("Progress: [");

  for (uint8_t i = 0; i < width; i++) {
    Serial.print(i < samplesInWindow ? "#" : "-");
  }

  Serial.printf("] %u/%u\n", samplesInWindow,  BUFFER_SIZE);
}

void printStatsBlock(
  const char* title,
  const Statistics& stats,
  const char* unit
) {
  Serial.println("----------------------------------------");
  Serial.println(title);
  Serial.printf("  Average: %.2f %s\n", stats.average, unit);
  Serial.printf("  Min:     %.2f %s\n", stats.min, unit);
  Serial.printf("  Max:     %.2f %s\n", stats.max, unit);
  Serial.printf("  Range:   %.2f %s\n", stats.range, unit);
}

void printReport() {
  float temperatures[BUFFER_SIZE];
  float humidities[BUFFER_SIZE];

  uint8_t validCount = 0;

  for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
    if (!readings[i].valid) {
      continue;
    }

    temperatures[validCount] = readings[i].temperature;
    humidities[validCount] = readings[i].humidity;
    validCount++;
  }

  uint8_t failedCount = BUFFER_SIZE - validCount;

  Serial.println();
  Serial.println("========================================");
  Serial.println("     1-MINUTE AVERAGE STATISTICS");
  Serial.println("========================================");

  if (validCount > 0) {
    Statistics tempStats =
      calculateStats(temperatures, validCount);

    Statistics humStats =
      calculateStats(humidities, validCount);

    printStatsBlock("TEMPERATURE", tempStats, "C");
    printStatsBlock("HUMIDITY", humStats, "%");
  } else {
    Serial.println("No valid measurements!");
  }

  Serial.println("----------------------------------------");
  Serial.printf(
    "Valid readings: %u/%u (%.1f%%)\n",
    validCount,
    BUFFER_SIZE,
    validCount * 100.0f / BUFFER_SIZE
  );

  Serial.printf("Failed readings: %u\n", failedCount);
  Serial.printf("Total read errors: %lu\n",
                (unsigned long)totalReadErrors);
  Serial.printf("Total retries: %lu\n",
                (unsigned long)totalRetries);
  Serial.printf("Total failed samples: %lu\n",
                (unsigned long)totalFailedSamples);

  Serial.println("========================================");
  Serial.println();
}

// ========== MEASUREMENT STORAGE ==========
void saveMeasurement(const Measurement& measurement) {
  readings[bufferIndex] = measurement;

  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  samplesInWindow++;
  totalSamples++;

  uint32_t elapsed =
    (uint32_t)(currentSlotStart - lastWindowStart)
    + READ_INTERVAL;

  Serial.printf(
    "[%02lu:%02lu] Reading %u/%u | ",
    (unsigned long)(elapsed / 60000),
    (unsigned long)((elapsed / 1000) % 60),
    samplesInWindow,
    BUFFER_SIZE
  );

  if (measurement.valid) {
    Serial.printf(
      "T: %.2f C | H: %.2f %% | OK\n",
      measurement.temperature,
      measurement.humidity
    );
  } else {
    totalFailedSamples++;
    Serial.println("INVALID / SENSOR ERROR");
  }

  printProgress();

  if (samplesInWindow == BUFFER_SIZE) {
    printReport();

    samplesInWindow = 0;
    lastWindowStart = currentSlotStart + READ_INTERVAL;

    Serial.println("Starting next 1-minute window...");
  }
}

// ========== SENSOR READING ==========
void attemptRead(uint32_t now) {
  currentAttempt++;
  lastAttemptTime = now;

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isValid(temperature, humidity)) {
    Measurement measurement;
    measurement.temperature = temperature;
    measurement.humidity = humidity;
    measurement.valid = true;

    saveMeasurement(measurement);
    readingInProgress = false;
    return;
  }

  totalReadErrors++;

  Serial.printf(
    "  Read error! Attempt %u/%u\n",
    currentAttempt,
    MAX_ATTEMPTS
  );

  if (currentAttempt >= MAX_ATTEMPTS) {
    Measurement measurement;

    saveMeasurement(measurement);
    readingInProgress = false;
  }
}

void updateSensor(uint32_t now) {
  if (readingInProgress) {
    if (now - lastAttemptTime >= RETRY_INTERVAL) {
      totalRetries++;
      attemptRead(now);
    }
    return;
  }

  if (now - currentSlotStart >= READ_INTERVAL) {
    currentSlotStart += READ_INTERVAL;
    currentAttempt = 0;
    readingInProgress = true;

    attemptRead(now);
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  lastWindowStart = millis();
  currentSlotStart = lastWindowStart;

  Serial.println();
  Serial.println("========================================");
  Serial.println("  DHT22 Averaging Monitoring System");
  Serial.println("========================================");
  Serial.println("Read interval: 5 seconds");
  Serial.println("Window: 12 measurements");
  Serial.println("Retries: 3 attempts");
  Serial.println();
}

void loop() {
  updateSensor(millis());
}
