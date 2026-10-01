#pragma once

#include <stddef.h>
#include <stdint.h>

class HidFraming {
public:
    static constexpr uint8_t kReportId = 6;
    static constexpr uint8_t kRpcChannel = 2;
    static constexpr size_t kPayloadSize = 63;
    static constexpr size_t kChunkSize = 61;

    static size_t encodeChunk(const char* message, size_t length, size_t offset,
                              uint8_t payload[kPayloadSize]);

    class Receiver {
    public:
        bool append(const uint8_t* payload, size_t length);
        bool takeLine(char* destination, size_t capacity);
        bool overflowed() const { return _overflowed; }
        void clear();

    private:
        bool promoteCompleteJson();
        static constexpr size_t kLineCapacity = 1024;
        char _building[kLineCapacity]{};
        char _ready[kLineCapacity]{};
        size_t _buildingLength = 0;
        bool _hasReady = false;
        bool _overflowed = false;
    };
};
