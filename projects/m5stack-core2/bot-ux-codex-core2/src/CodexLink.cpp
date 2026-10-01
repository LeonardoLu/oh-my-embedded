#include "CodexLink.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>

#include <stdio.h>
#include <string.h>

namespace {
constexpr char kDeviceName[] = "Core2 Codex Micro";
constexpr char kManufacturer[] = "Work Louder Core2 Emulator";
constexpr char kFirmwareVersion[] = "core2-emulator-0.1.0";
constexpr uint16_t kVendorId = 0x303A;
constexpr uint16_t kProductId = 0x8360;
constexpr uint16_t kProductVersion = 0x0101; // Low bits distinguish BLE from USB.
constexpr uint8_t kTapReleaseDelayMs = 24;

// Vendor-defined 63-byte input/output reports. BLE carries the report ID in
// the Report Reference descriptor, so characteristic values contain bytes 1..63.
uint8_t kReportMap[] = {
    // Idle keyboard report makes macOS offer HOGP enrollment. Firmware never
    // sends this report; Codex actions remain on the vendor collection below.
    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x06,             // Usage (Keyboard)
    0xA1, 0x01,             // Collection (Application)
    0x85, 0x01,             // Report ID (1)
    0x05, 0x07,             // Usage Page (Keyboard)
    0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08,
    0x81, 0x02,             // Input (modifier bits)
    0x95, 0x01, 0x75, 0x08,
    0x81, 0x01,             // Input (reserved)
    0x95, 0x05, 0x75, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x91, 0x02,             // Output (LEDs)
    0x95, 0x01, 0x75, 0x03,
    0x91, 0x01,             // Output (padding)
    0x95, 0x06, 0x75, 0x08,
    0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
    0x81, 0x00,             // Input (six key slots)
    0xC0,

    0x06, 0x00, 0xFF,       // Usage Page (Vendor 0xFF00)
    0x09, 0x01,             // Usage (1)
    0xA1, 0x01,             // Collection (Application)
    0x85, HidFraming::kReportId,
    0x15, 0x00,             // Logical Minimum (0)
    0x26, 0xFF, 0x00,       // Logical Maximum (255)
    0x75, 0x08,             // Report Size (8)
    0x95, HidFraming::kPayloadSize,
    0x09, 0x01,
    0x81, 0x02,             // Input (Data, Variable, Absolute)
    0x95, HidFraming::kPayloadSize,
    0x09, 0x01,
    0x91, 0x02,             // Output (Data, Variable, Absolute)
    0xC0,
};

CodexLink* activeLink = nullptr;
portMUX_TYPE receiverMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE connectionMux = portMUX_INITIALIZER_UNLOCKED;

uint8_t normalizedByte(JsonVariantConst value, uint8_t fallback)
{
    if (value.isNull()) return fallback;
    float number = value.as<float>();
    if (number <= 1.0f) number *= 255.0f;
    if (number < 0.0f) number = 0.0f;
    if (number > 255.0f) number = 255.0f;
    return static_cast<uint8_t>(number + 0.5f);
}

void updateZone(LightingZone& zone, JsonObjectConst object)
{
    if (!object["c"].isNull()) zone.color = object["c"].as<uint32_t>() & 0xFFFFFFu;
    zone.brightness = normalizedByte(object["b"], zone.brightness);
    if (!object["e"].isNull()) zone.effect = object["e"].as<uint8_t>();
    zone.speed = normalizedByte(object["s"], zone.speed);
}

class LinkServerCallbacks : public BLEServerCallbacks {
public:
    void onConnect(BLEServer*) override
    {
        if (activeLink) activeLink->handleConnection(true);
    }

    void onDisconnect(BLEServer*) override
    {
        if (activeLink) activeLink->handleConnection(false);
    }

    void onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t* params) override
    {
        if (activeLink && params) activeLink->handleMtu(params->mtu.mtu);
    }
};

class LinkOutputCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* characteristic) override
    {
        if (activeLink)
            activeLink->receiveOutputReport(characteristic->getData(), characteristic->getLength());
    }
};

LinkServerCallbacks serverCallbacks;
LinkOutputCallbacks outputCallbacks;
}

bool CodexLink::begin()
{
    activeLink = this;
    BLEDevice::init(kDeviceName);
    BLEDevice::setMTU(67);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    _server = BLEDevice::createServer();
    if (!_server) return false;
    _server->setCallbacks(&serverCallbacks);

    _hid = new BLEHIDDevice(_server);
    if (!_hid) return false;
    _keyboardInput = _hid->inputReport(1);
    _keyboardOutput = _hid->outputReport(1);
    _inputReport = _hid->inputReport(HidFraming::kReportId);
    _outputReport = _hid->outputReport(HidFraming::kReportId);
    if (!_keyboardInput || !_keyboardOutput || !_inputReport || !_outputReport) return false;
    _outputReport->setCallbacks(&outputCallbacks);

    BLECharacteristic* manufacturer = _hid->manufacturer();
    if (manufacturer) manufacturer->setValue(kManufacturer);
    // The pinned ESP32 BLE library serializes these 16-bit PnP fields high-byte
    // first. Swap arguments so the DIS characteristic contains the required LE
    // bytes 3A 30 / 60 83 for VID 0x303A and PID 0x8360.
    _hid->pnp(0x02, static_cast<uint16_t>((kVendorId << 8) | (kVendorId >> 8)),
              static_cast<uint16_t>((kProductId << 8) | (kProductId >> 8)),
              kProductVersion); // USB-IF VID source.
    _hid->hidInfo(0x00, 0x02); // Normally connectable.
    _hid->reportMap(kReportMap, sizeof(kReportMap));
    _hid->startServices();
    _hid->setBatteryLevel(100);

    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    BLEAdvertising* advertising = _server->getAdvertising();
    BLEAdvertisementData advertisement;
    advertisement.setFlags(0x06); // General discoverable, BR/EDR unsupported.
    advertisement.setCompleteServices(BLEUUID(static_cast<uint16_t>(0x1812)));
    advertisement.setAppearance(HID_KEYBOARD);
    advertisement.setShortName(kDeviceName);
    advertising->setAdvertisementData(advertisement);
    BLEAdvertisementData scanResponse;
    scanResponse.setName(kDeviceName);
    advertising->setScanResponseData(scanResponse);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    advertising->start();
    _state = State::Advertising;
    return true;
}

void CodexLink::handleConnection(bool connected)
{
    portENTER_CRITICAL(&connectionMux);
    _connectionEventConnected = connected;
    _connectionEventPending = true;
    portEXIT_CRITICAL(&connectionMux);
}

void CodexLink::handleMtu(uint16_t mtu)
{
    portENTER_CRITICAL(&connectionMux);
    _mtuEventValue = mtu;
    _mtuEventPending = true;
    portEXIT_CRITICAL(&connectionMux);
}

void CodexLink::receiveOutputReport(const uint8_t* payload, size_t length)
{
    // Most BLE stacks remove the report ID using the Report Reference descriptor.
    // Accept it when a host leaves it at the front of the characteristic value.
    if (length == HidFraming::kPayloadSize + 1 && payload && payload[0] == HidFraming::kReportId)
    {
        ++payload;
        --length;
    }
    portENTER_CRITICAL(&receiverMux);
    _receiver.append(payload, length);
    portEXIT_CRITICAL(&receiverMux);
}

void CodexLink::update(int8_t batteryPercent, bool charging, uint32_t nowMs)
{
    (void)charging;
    bool hasConnectionEvent = false;
    bool eventConnected = false;
    bool hasMtuEvent = false;
    uint16_t eventMtu = 23;
    portENTER_CRITICAL(&connectionMux);
    if (_connectionEventPending)
    {
        hasConnectionEvent = true;
        eventConnected = _connectionEventConnected;
        _connectionEventPending = false;
    }
    if (_mtuEventPending)
    {
        hasMtuEvent = true;
        eventMtu = _mtuEventValue;
        _mtuEventPending = false;
    }
    portEXIT_CRITICAL(&connectionMux);

    if (hasConnectionEvent)
    {
        _state = eventConnected ? State::Connected : State::Advertising;
        _threadLightingFresh = false;
        _lighting = LightingState{};
        ++_lightingRevision;
        _peerMtu = 23;
        if (!eventConnected)
        {
            portENTER_CRITICAL(&receiverMux);
            _receiver.clear();
            portEXIT_CRITICAL(&receiverMux);
            _controlReady = false;
            _sawRpc = false;
            _pendingReleaseKey[0] = '\0';
            if (_server) _server->startAdvertising();
        }
    }
    if (hasMtuEvent && _state == State::Connected) _peerMtu = eventMtu;
    _refreshTransportState();

    if (_pendingReleaseKey[0] && static_cast<int32_t>(nowMs - _releaseAtMs) >= 0)
    {
        _sendKey(_pendingReleaseKey, 0, _pendingReleaseAgent);
        _pendingReleaseKey[0] = '\0';
    }

    if (_hid && batteryPercent >= 0 && batteryPercent <= 100 && batteryPercent != _lastBattery)
    {
        _lastBattery = batteryPercent;
        _hid->setBatteryLevel(static_cast<uint8_t>(batteryPercent));
    }

    char line[1024];
    while (true)
    {
        portENTER_CRITICAL(&receiverMux);
        const bool ready = _receiver.takeLine(line, sizeof(line));
        portEXIT_CRITICAL(&receiverMux);
        if (!ready) break;
        ++_receivedRpcCount;
        _processLine(line, batteryPercent, charging);
    }
}

bool CodexLink::_sendMessage(const char* message)
{
    if (!connected() || _disconnectPending() || !_inputReport || !message) return false;
    _refreshTransportState();
    if (!_controlReady) return false;
    const size_t length = strlen(message);
    size_t offset = 0;
    uint8_t payload[HidFraming::kPayloadSize];
    while (offset < length)
    {
        if (_disconnectPending()) return false;
        const size_t count = HidFraming::encodeChunk(message, length, offset, payload);
        if (!count) return false;
        _inputReport->setValue(payload, sizeof(payload));
        _inputReport->notify();
        offset += count;
        if (offset < length) delay(3);
    }
    return true;
}

bool CodexLink::_disconnectPending() const
{
    bool pending = false;
    portENTER_CRITICAL(&connectionMux);
    pending = _connectionEventPending && !_connectionEventConnected;
    portEXIT_CRITICAL(&connectionMux);
    return pending;
}

void CodexLink::_refreshTransportState()
{
    _controlReady = false;
    if (!connected() || !_inputReport || !_server || !_sawRpc) return;
    BLE2902* notifications = static_cast<BLE2902*>(
        _inputReport->getDescriptorByUUID(BLEUUID(static_cast<uint16_t>(0x2902))));
    if (!notifications || !notifications->getNotifications()) return;
    _controlReady = _peerMtu >= HidFraming::kPayloadSize + 3;
}

bool CodexLink::_sendKey(const char* key, uint8_t action, int8_t agent)
{
    if (!key || strlen(key) >= sizeof(_pendingReleaseKey)) return false;
    char message[112];
    if (agent >= 0)
        snprintf(message, sizeof(message),
                 "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"%s\",\"act\":%u,\"ag\":%d}}\n",
                 key, static_cast<unsigned>(action), static_cast<int>(agent));
    else
        snprintf(message, sizeof(message),
                 "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"%s\",\"act\":%u}}\n",
                 key, static_cast<unsigned>(action));
    if (!_sendMessage(message)) return false;
    ++_sentEventCount;
    return true;
}

bool CodexLink::tapKey(const char* key, int8_t agent)
{
    if (_pendingReleaseKey[0] || !_sendKey(key, 1, agent)) return false;
    strncpy(_pendingReleaseKey, key, sizeof(_pendingReleaseKey) - 1);
    _pendingReleaseKey[sizeof(_pendingReleaseKey) - 1] = '\0';
    _pendingReleaseAgent = agent;
    _releaseAtMs = millis() + kTapReleaseDelayMs;
    return true;
}

bool CodexLink::pressKey(const char* key, int8_t agent) { return _sendKey(key, 1, agent); }
bool CodexLink::releaseKey(const char* key, int8_t agent) { return _sendKey(key, 0, agent); }

bool CodexLink::turnEncoder(bool clockwise)
{
    const char* key = clockwise ? "ENC_CW" : "ENC_CC";
    char message[96];
    snprintf(message, sizeof(message),
             "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"%s\",\"act\":2}}\n", key);
    if (!_sendMessage(message)) return false;
    ++_sentEventCount;
    return true;
}

bool CodexLink::moveJoystick(float angle, float distance)
{
    if (angle < 0.0f) angle = 0.0f;
    if (angle >= 1.0f) angle = 0.0f;
    if (distance < 0.0f) distance = 0.0f;
    if (distance > 1.0f) distance = 1.0f;
    char message[80];
    snprintf(message, sizeof(message),
             "{\"m\":\"v.oai.rad\",\"p\":{\"a\":%.4f,\"d\":%.4f}}\n",
             static_cast<double>(angle), static_cast<double>(distance));
    if (!_sendMessage(message)) return false;
    ++_sentEventCount;
    return true;
}

void CodexLink::_sendRpcResult(const char* idJson, const char* resultJson)
{
    char message[256];
    snprintf(message, sizeof(message), "{\"id\":%s,\"result\":%s}\n", idJson, resultJson);
    _sendMessage(message);
}

void CodexLink::_sendRpcError(const char* idJson, int code, const char* messageText)
{
    char message[256];
    snprintf(message, sizeof(message),
             "{\"id\":%s,\"error\":{\"code\":%d,\"message\":\"%s\"}}\n",
             idJson, code, messageText);
    _sendMessage(message);
}

void CodexLink::_processLine(const char* line, int8_t batteryPercent, bool charging)
{
    JsonDocument request;
    if (deserializeJson(request, line)) return;
    const char* method = request["method"].is<const char*>()
                             ? request["method"].as<const char*>()
                             : request["m"] | "";
    JsonVariantConst id = request["id"];
    if (id.isNull()) id = request["i"];
    if (id.isNull() || !method[0]) return;

    _sawRpc = true;
    _refreshTransportState();

    char idJson[32];
    serializeJson(id, idJson, sizeof(idJson));

    if (strcmp(method, "sys.version") == 0)
    {
        char result[96];
        snprintf(result, sizeof(result), "{\"version\":\"%s\"}", kFirmwareVersion);
        _sendRpcResult(idJson, result);
    }
    else if (strcmp(method, "device.status") == 0)
    {
        char result[192];
        snprintf(result, sizeof(result),
                 "{\"version\":\"%s\",\"profile_index\":0,\"layer_index\":0,"
                 "\"battery\":%d,\"is_charging\":%s}",
                 kFirmwareVersion, batteryPercent >= 0 ? batteryPercent : 0,
                 charging ? "true" : "false");
        _sendRpcResult(idJson, result);
    }
    else if (strcmp(method, "v.oai.thstatus") == 0)
    {
        JsonArrayConst items = request["params"].as<JsonArrayConst>();
        for (JsonObjectConst item : items)
        {
            const int slot = item["id"] | -1;
            if (slot >= 0 && slot < LightingState::kSlotCount)
                updateZone(_lighting.slots[slot], item);
        }
        _threadLightingFresh = true;
        ++_lightingRevision;
        _sendRpcResult(idJson, "null");
    }
    else if (strcmp(method, "v.oai.rgbcfg") == 0)
    {
        JsonObjectConst params = request["params"].as<JsonObjectConst>();
        if (params["keys"].is<JsonObjectConst>())
            updateZone(_lighting.keys, params["keys"].as<JsonObjectConst>());
        if (params["ambient"].is<JsonObjectConst>())
            updateZone(_lighting.ambient, params["ambient"].as<JsonObjectConst>());
        ++_lightingRevision;
        _sendRpcResult(idJson, "null");
    }
    else if (strcmp(method, "sys.bootloader") == 0 || strcmp(method, "sys.selftest") == 0)
    {
        _sendRpcError(idJson, -32000, "Unsupported on Core2 emulator");
    }
    else
    {
        _sendRpcError(idJson, -32601, "Method not supported by Core2 emulator");
    }
}
