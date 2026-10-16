#pragma once

#include <Arduino.h>
#include <vector>

class MorseTransmitter {
    public:
        explicit MorseTransmitter(uint8_t ledPin);

        void init();
        void update();
        void start();
        void stop();
        void toggle();
        void setMessage(String message);
        void setSpeed(uint16_t percent);
        void printStatus() const;

        bool isRunning() const;

        uint32_t getSosCount() const;

    private:
        struct Signal {
            bool ledOn;
            uint16_t duration;
            char symbol;
            char letter;
        };

        uint8_t _ledPin;
        bool _running = false;

        String _message = "SOS";
        uint16_t _speed = 100;

        uint32_t _sosCount = 0;
        uint32_t _messageCount = 0;

        std::vector<Signal> _signals;

        size_t _index = 0;
        uint32_t _startedAt = 0;

        const char* getMorse(char c) const;

        void buildSignals();
        void applySignal();
        
        uint32_t scaledDuration(uint16_t duration) const;
};