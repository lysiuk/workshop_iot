#include <Arduino.h>
#include "MorseTransmitter.h"

constexpr uint8_t LED_PIN = 13;
constexpr uint8_t BUTTON_PIN = 0;

MorseTransmitter morse(LED_PIN);

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

uint32_t lastDebounceTime = 0;
constexpr uint32_t DEBOUNCE_MS = 50;

String inputBuffer;

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > DEBOUNCE_MS) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        morse.toggle();
      }
    }
  }

  lastButtonReading = reading;
}

void handleCommand(String command) {
  command.trim();

  String upper = command;
  upper.toUpperCase();

  if (upper == "START") {
    morse.start();
  }
  else if (upper == "STOP") {
    morse.stop();
  }
  else if (upper == "STATUS") {
    morse.printStatus();
  }
  else if (upper.startsWith("MSG ")) {
    morse.setMessage(
      command.substring(4)
    );
  }
  else if (upper.startsWith("SPEED ")) {
    String value = command.substring(6);
    value.trim();

    bool valid = !value.isEmpty();

    for (size_t i = 0; i < value.length(); i++) {
      if (!isDigit(value[i])) {
        valid = false;
        break;
      }
    }

    if (valid) {
      unsigned long speed = value.toInt();

      if (speed >= 25 && speed <= 400) {
        morse.setSpeed(
          static_cast<uint16_t>(speed)
        );
      } else {
        Serial.println("[ERROR] Speed: 25..400");
      }
    } else {
      Serial.println("[ERROR] Invalid speed");
    }
  }
  else {
    Serial.println("[ERROR] Unknown command");
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (!inputBuffer.isEmpty()) {
        handleCommand(inputBuffer);
        inputBuffer = "";
      }
    }
    else if (inputBuffer.length() < 128) {
      inputBuffer += c;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  morse.init();
  morse.setMessage("SOS");

  Serial.println("\n=== MORSE TRANSMITTER ===");
  Serial.println("BOOT button: START / STOP");
  Serial.println("Commands:");
  Serial.println("  START");
  Serial.println("  STOP");
  Serial.println("  MSG HELLO WORLD");
  Serial.println("  SPEED 150");
  Serial.println("  STATUS");

  Serial.println("\nReady!");
}

void loop() {
  handleButton();
  handleSerial();
  morse.update();
}