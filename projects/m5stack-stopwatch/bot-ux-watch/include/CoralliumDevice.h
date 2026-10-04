#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Power.h"
#include "CoralliumEventGate.h"

// Transport callbacks only enqueue complete frames. RTC and NVS work
// runs on the existing main task, never on NimBLE's host callback task.
class CoralliumDevice {
public:
    void begin(Power& power);
    void update(uint32_t now);
    void openWindow();
    void closeWindow();
    bool windowOpen() const { return _windowUntil && (int32_t)(_windowUntil-millis())>0; }
    bool connected() const { return _connected; }
    bool active() const;
    void setClockEditing(bool busy) { _clockEditing=busy; }
    bool serialByte(char c);
    uint32_t remainingSeconds() const {
        int32_t remaining=(int32_t)(_windowUntil-millis());
        return _windowUntil&&remaining>0?(uint32_t)remaining/1000:0;
    }
private:
    class ServerCallbacks : public NimBLEServerCallbacks {
        void onConnect(NimBLEServer*,ble_gap_conn_desc*) override;
        void onDisconnect(NimBLEServer*) override;
        void onAuthenticationComplete(ble_gap_conn_desc*) override;
    };
    class RxCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic*) override;
    };
    struct Message { char line[2049]; };
    static CoralliumDevice* _instance;
    ServerCallbacks _serverCallbacks;
    RxCallbacks _rxCallbacks;
    NimBLEServer* _server=nullptr;
    NimBLECharacteristic* _tx=nullptr;
    QueueHandle_t _requests=nullptr;
    Power* _power=nullptr;
    Message _pending{};
    volatile bool _connected=false, _encrypted=false, _disconnected=false;
    volatile uint16_t _connection=0;
    uint32_t _windowUntil=0, _lastNotify=0;
    corallium::EventGate _events;
    int16_t _offset=0;
    bool _started=false, _offsetKnown=false, _clockEditing=false;
    char _rxLine[2049]={}, _serialLine[2049]={}, _output[2049]={};
    size_t _rxLength=0, _serialLength=0, _outputLength=0, _outputOffset=0;
    bool _rxOverflow=false, _serialOverflow=false, _serialReading=false;
    void receive(const std::string& bytes);
    void process(const char* line,bool serial);
    void send(JsonDocument& doc,bool serial);
    void timeStatus(JsonObject object);
    void status(JsonObject object);
};
