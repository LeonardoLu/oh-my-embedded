#pragma once

#include <stddef.h>
#include <stdint.h>

#include "HidFraming.h"
#include "LightingState.h"

class BLECharacteristic;
class BLEHIDDevice;
class BLEServer;

class CodexLink {
public:
    enum class State : uint8_t { Starting, Advertising, Connected };

    bool begin();
    void update(int8_t batteryPercent, bool charging, uint32_t nowMs);

    bool tapKey(const char* key, int8_t agent = -1);
    bool pressKey(const char* key, int8_t agent = -1);
    bool releaseKey(const char* key, int8_t agent = -1);
    bool turnEncoder(bool clockwise);
    bool moveJoystick(float angle, float distance);

    State state() const { return _state; }
    bool connected() const { return _state == State::Connected; }
    bool controlReady() const { return connected() && _controlReady; }
    uint16_t peerMtu() const { return _peerMtu; }
    const LightingState& lighting() const { return _lighting; }
    uint32_t receivedRpcCount() const { return _receivedRpcCount; }
    uint32_t sentEventCount() const { return _sentEventCount; }
    uint32_t lightingRevision() const { return _lightingRevision; }
    bool threadLightingFresh() const { return _threadLightingFresh; }

    // BLE callback entry points.
    void handleConnection(bool connected);
    void handleMtu(uint16_t mtu);
    void receiveOutputReport(const uint8_t* payload, size_t length);

private:
    bool _sendMessage(const char* message);
    bool _sendKey(const char* key, uint8_t action, int8_t agent);
    void _processLine(const char* line, int8_t batteryPercent, bool charging);
    void _sendRpcResult(const char* idJson, const char* resultJson);
    void _sendRpcError(const char* idJson, int code, const char* message);
    void _refreshTransportState();
    bool _disconnectPending() const;

    BLEServer* _server = nullptr;
    BLEHIDDevice* _hid = nullptr;
    BLECharacteristic* _keyboardInput = nullptr;
    BLECharacteristic* _keyboardOutput = nullptr;
    BLECharacteristic* _inputReport = nullptr;
    BLECharacteristic* _outputReport = nullptr;
    HidFraming::Receiver _receiver;
    LightingState _lighting{};
    State _state = State::Starting;
    volatile bool _connectionEventPending = false;
    volatile bool _connectionEventConnected = false;
    volatile bool _mtuEventPending = false;
    volatile uint16_t _mtuEventValue = 23;
    bool _controlReady = false;
    bool _sawRpc = false;
    uint16_t _peerMtu = 23;
    int8_t _lastBattery = -1;
    char _pendingReleaseKey[16]{};
    int8_t _pendingReleaseAgent = -1;
    uint32_t _releaseAtMs = 0;
    uint32_t _receivedRpcCount = 0;
    uint32_t _sentEventCount = 0;
    uint32_t _lightingRevision = 0;
    bool _threadLightingFresh = false;
};
