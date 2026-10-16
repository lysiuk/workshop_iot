#include "MorseTransmitter.h"

MorseTransmitter::MorseTransmitter(uint8_t ledPin) : _ledPin(ledPin) {}

void MorseTransmitter::init() {
    pinMode(_ledPin, OUTPUT);
    digitalWrite(_ledPin, LOW);

    buildSignals();
}

const char* MorseTransmitter::getMorse(char c) const {
    static const char* codes[] = {
        ".-", "-...", "-.-.", "-..", ".", "..-.",
        "--.", "....", "..", ".---", "-.-", ".-..",
        "--", "-.", "---", ".--.", "--.-", ".-.",
        "...", "-", "..-", "...-", ".--", "-..-",
        "-.--", "--.."
    };

    static const char* digits[] = {
        "-----", ".----", "..---", "...--", "....-",
        ".....", "-....", "--...", "---..", "----."
    };

    if (c >= 'A' && c <= 'Z') {
        return codes[c - 'A'];
    }

    if (c >= '0' && c <= '9') {
        return digits[c - '0'];
    }

    return nullptr;
}

void MorseTransmitter::buildSignals() {
    _signals.clear();

    // Remove white spaces
    _message.trim();
    _message.toUpperCase();

    bool firstLetter = true;
    bool wordGap = false;

    for (size_t i = 0; i < _message.length(); i++) {
        char letter = _message[i];

        if (letter == ' ') {
            wordGap = true;
            continue;
        }

        const char* code = getMorse(letter);

        if (code == nullptr) {
            continue;
        }

        // Pause between letters or words
        if (!firstLetter) {
            _signals.push_back({
                false,
                static_cast<uint16_t>(
                    wordGap ? 1400 : 600
                ),
                ' ',
                letter
            });
        }

        firstLetter = false;
        wordGap = false;

        for (size_t j = 0; code[j] != '\0'; j++) {
            char symbol = code[j];

            // Dot or dash
            _signals.push_back({
                true,
                static_cast<uint16_t>(
                    symbol == '.' ? 200 : 600
                ),
                symbol,
                letter
            });

            // Pause between letter elements
            if (code[j + 1] != '\0') {
                _signals.push_back({
                    false, 200, ' ', letter
                });
            }
        }
    }

    // Pause between messages 
    if (!_signals.empty()) {
        _signals.push_back({
            false, 1400, ' ', ' '
        });
    }

    _index = 0;
}

uint32_t MorseTransmitter::scaledDuration(uint16_t duration) const {
    return static_cast<uint32_t>(duration) * 100 / _speed;
}

void MorseTransmitter::applySignal() {
    const Signal& signal = _signals[_index];

    digitalWrite(
        _ledPin,
        signal.ledOn ? HIGH : LOW
    );

    if (signal.ledOn) {
        Serial.printf(
            "[MORSE] %c : %c (%lu ms)\n",
            signal.letter,
            signal.symbol,
            static_cast<unsigned long>(
                scaledDuration(signal.duration)
            )
        );
    }
}

void MorseTransmitter::update() {
    if (!_running || _signals.empty()) {
        return;
    }

    const Signal& signal = _signals[_index];

    if (millis() - _startedAt <
        scaledDuration(signal.duration)) {
        return;
    }

    _index++;

    if (_index >= _signals.size()) {
        _index = 0;
        _messageCount++;

        if (_message == "SOS") {
            _sosCount++;
        }

        Serial.printf(
            "\n[DONE] Messages counter: %lu | Msg SOS counter: %lu\n\n",
            static_cast<unsigned long>(_messageCount),
            static_cast<unsigned long>(_sosCount)
        );
    }

    _startedAt = millis();
    applySignal();
}

void MorseTransmitter::start() {
    if (_running || _signals.empty()) {
        return;
    }

    _running = true;
    _index = 0;
    _startedAt = millis();

    Serial.println("\n[START] " + _message);
    applySignal();
}

void MorseTransmitter::stop() {
    if (!_running) {
        return;
    }

    _running = false;
    digitalWrite(_ledPin, LOW);

    Serial.println("[STOP] Transmission stopped");
}

void MorseTransmitter::toggle() {
    if (_running) {
        stop();
    } else {
        start();
    }
}

void MorseTransmitter::setMessage(String message) {
    message.trim();
    message.toUpperCase();

    if (message.isEmpty()) {
        Serial.println("[ERROR] Empty message");
        return;
    }

    stop();
    _message = message;

    buildSignals();

    Serial.println("[MSG] " + _message);
}

void MorseTransmitter::setSpeed(uint16_t percent) {
    if (percent < 25 || percent > 400) {
        Serial.println(
            "[ERROR] Speed must be 25..400"
        );
        return;
    }

    bool wasRunning = _running;
    stop();

    _speed = percent;

    Serial.printf(
        "[SPEED] %u%%\n", _speed
    );

    if (wasRunning) {
        start();
    }
}

bool MorseTransmitter::isRunning() const {
    return _running;
}

uint32_t MorseTransmitter::getSosCount() const {
    return _sosCount;
}

void MorseTransmitter::printStatus() const {
    Serial.println("\n--- STATUS ---");
    Serial.println("Message: " + _message);

    Serial.printf("Speed: %u%%\n", _speed);

    Serial.printf(
        "Running: %s\n",
        _running ? "YES" : "NO"
    );

    Serial.printf(
        "Messages: %lu\n",
        static_cast<unsigned long>(_messageCount)
    );

    Serial.printf(
        "SOS count: %lu\n",
        static_cast<unsigned long>(_sosCount)
    );

    Serial.println("--------------\n");
}