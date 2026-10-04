#include "HidFraming.h"

#include <string.h>

size_t HidFraming::encodeChunk(const char* message, size_t length, size_t offset,
                               uint8_t payload[kPayloadSize])
{
    memset(payload, 0, kPayloadSize);
    if (!message || offset >= length) return 0;
    const size_t remaining = length - offset;
    const size_t chunk = remaining < kChunkSize ? remaining : kChunkSize;
    payload[0] = kRpcChannel;
    payload[1] = static_cast<uint8_t>(chunk);
    memcpy(payload + 2, message + offset, chunk);
    return chunk;
}

bool HidFraming::Receiver::append(const uint8_t* payload, size_t length)
{
    if (!payload || length < 2 || payload[0] != kRpcChannel) return false;
    const size_t chunkLength = payload[1];
    if (chunkLength > kChunkSize || chunkLength + 2 > length) return false;

    for (size_t i = 0; i < chunkLength; ++i)
    {
        const char value = static_cast<char>(payload[i + 2]);
        if (value == '\n')
        {
            if (_hasReady)
            {
                _overflowed = true;
                _buildingLength = 0;
                return false;
            }
            _building[_buildingLength] = '\0';
            memcpy(_ready, _building, _buildingLength + 1);
            _hasReady = true;
            _buildingLength = 0;
        }
        else if (value != '\r')
        {
            if (_buildingLength + 1 >= kLineCapacity)
            {
                _overflowed = true;
                _buildingLength = 0;
                return false;
            }
            _building[_buildingLength++] = value;
        }
    }
    const bool valid = promoteCompleteJson();
    if (valid && chunkLength < kChunkSize && !_hasReady && _buildingLength)
    {
        clear();
        _overflowed = true;
        return false;
    }
    return valid;
}

bool HidFraming::Receiver::promoteCompleteJson()
{
    if (_hasReady) return true;
    bool inString = false;
    bool escaped = false;
    bool started = false;
    int depth = 0;
    for (size_t i = 0; i < _buildingLength; ++i)
    {
        const char value = _building[i];
        if (inString)
        {
            if (escaped) escaped = false;
            else if (value == '\\') escaped = true;
            else if (value == '"') inString = false;
            continue;
        }
        if (value == '"') { inString = true; continue; }
        if (value == '{' || value == '[') { ++depth; started = true; }
        else if (value == '}' || value == ']')
        {
            if (--depth < 0) { clear(); _overflowed = true; return false; }
            if (started && depth == 0)
            {
                for (size_t tail = i + 1; tail < _buildingLength; ++tail)
                    if (_building[tail] != ' ' && _building[tail] != '\t')
                    { clear(); _overflowed = true; return false; }
                _building[i + 1] = '\0';
                memcpy(_ready, _building, i + 2);
                _hasReady = true;
                _buildingLength = 0;
                return true;
            }
        }
    }
    return true;
}

bool HidFraming::Receiver::takeLine(char* destination, size_t capacity)
{
    if (!_hasReady || !destination || capacity == 0) return false;
    const size_t length = strlen(_ready);
    const size_t copied = length < capacity - 1 ? length : capacity - 1;
    memcpy(destination, _ready, copied);
    destination[copied] = '\0';
    _hasReady = false;
    return copied == length;
}

void HidFraming::Receiver::clear()
{
    _buildingLength = 0;
    _hasReady = false;
    _overflowed = false;
    _building[0] = '\0';
    _ready[0] = '\0';
}
