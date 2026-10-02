#include "CoralliumDevice.h"
#include "CoralliumTime.h"
#include "CoralliumClockTransaction.h"
#include "CoralliumValidation.h"
#include "RtcClock.h"
#include <Preferences.h>
#include <esp_timer.h>

namespace {
constexpr char Service[]="7D2A0001-7E6D-4C55-A8D2-6B6D5E8F9010";
constexpr char Receive[]="7D2A0002-7E6D-4C55-A8D2-6B6D5E8F9010";
constexpr char Transmit[]="7D2A0003-7E6D-4C55-A8D2-6B6D5E8F9010";
bool validId(const char* id) {
    if(!id||!id[0]||strlen(id)>64) return false;
    for(const char* p=id;*p;++p)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='_'||*p=='-')) return false;
    return true;
}
}
CoralliumDevice* CoralliumDevice::_instance=nullptr;
void CoralliumDevice::begin(Power& power) {
    _instance=this; _power=&power;
    _requests=xQueueCreate(2,sizeof(Message));
    Preferences prefs; prefs.begin("corallium",true);
    _offsetKnown=prefs.getBool("utcValid",prefs.isKey("utcOffset"));
    _offset=prefs.getShort("utcOffset",0); prefs.end();
    if(_offset< -720||_offset>840) _offset=0;
}
void CoralliumDevice::openWindow() {
    if(!_requests) return;
    if(!_started) {
        NimBLEDevice::init("StopWatch");
        NimBLEDevice::setSecurityAuth(true,false,true);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        _server=NimBLEDevice::createServer(); _server->setCallbacks(&_serverCallbacks,false);
        _server->advertiseOnDisconnect(false);
        auto* service=_server->createService(Service);
        auto* rx=service->createCharacteristic(Receive,NIMBLE_PROPERTY::WRITE|NIMBLE_PROPERTY::WRITE_ENC,2048);
        rx->setCallbacks(&_rxCallbacks);
        _tx=service->createCharacteristic(Transmit,NIMBLE_PROPERTY::NOTIFY,2048);
        service->start();
        auto* advertising=NimBLEDevice::getAdvertising();
        advertising->addServiceUUID(Service); advertising->setScanResponse(true);
        _started=true;
    }
    _windowUntil=millis()+300000;
    if(!_connected) NimBLEDevice::startAdvertising();
}
void CoralliumDevice::closeWindow() {
    _windowUntil=0;
    _events.reset();
    if(!_started) return;
    _encrypted=false;
    NimBLEDevice::stopAdvertising();
    if(_connected) _server->disconnect(_connection);
    _outputLength=_outputOffset=0;
    xQueueReset(_requests);
}
void CoralliumDevice::ServerCallbacks::onConnect(NimBLEServer* server,ble_gap_conn_desc* desc) {
    auto* self=_instance;
    self->_events.reset();
    if(!self->windowOpen()) { server->disconnect(desc->conn_handle); return; }
    self->_connected=true; self->_encrypted=false; self->_connection=desc->conn_handle;
    NimBLEDevice::startSecurity(desc->conn_handle);
}
void CoralliumDevice::ServerCallbacks::onDisconnect(NimBLEServer*) {
    auto* self=_instance; self->_connected=self->_encrypted=false;
    self->_events.reset();
    memset(self->_rxLine,0,sizeof(self->_rxLine));
    self->_rxLength=0; self->_rxOverflow=false; self->_disconnected=true;
}
void CoralliumDevice::ServerCallbacks::onAuthenticationComplete(ble_gap_conn_desc* desc) {
    auto* self=_instance; self->_encrypted=desc->sec_state.encrypted&&self->windowOpen();
    if(!self->_encrypted) self->_server->disconnect(desc->conn_handle);
}
void CoralliumDevice::RxCallbacks::onWrite(NimBLECharacteristic* characteristic) {
    auto* self=_instance;
    if(self->_encrypted&&self->windowOpen()) self->receive(characteristic->getValue());
}
void CoralliumDevice::receive(const std::string& bytes) {
    for(char c:bytes) {
        if(c=='\n') {
            if(!_rxOverflow&&_rxLength) {
                _rxLine[_rxLength]=0;
                if(xQueueSend(_requests,_rxLine,0)!=pdTRUE) {
                    // No silent execution when a sender violates serialization.
                    _server->disconnect(_connection);
                }
            }
            memset(_rxLine,0,sizeof(_rxLine)); _rxLength=0; _rxOverflow=false;
        } else if(!_rxOverflow) {
            if(!c) { _rxOverflow=true; continue; }
            if(_rxLength<2048) _rxLine[_rxLength++]=c;
            else _rxOverflow=true;
        }
    }
}
bool CoralliumDevice::serialByte(char c) {
    if(!_serialReading) {
        if(c!='{') return false;
        _serialReading=true; _serialLength=0; _serialOverflow=false;
    }
    if(c=='\n') {
        if(!_serialOverflow) { _serialLine[_serialLength]=0; process(_serialLine,true); }
        memset(_serialLine,0,sizeof(_serialLine)); _serialReading=false; _serialLength=0;
    } else if(!_serialOverflow) {
        if(!c) { _serialOverflow=true; return true; }
        if(_serialLength<2048) _serialLine[_serialLength++]=c;
        else _serialOverflow=true;
    }
    return true;
}
bool CoralliumDevice::active() const { return windowOpen()||_connected; }
void CoralliumDevice::timeStatus(JsonObject object) {
    bool valid=watchClock.refresh()&&watchClock.state()==RtcClock::State::Ready&&_offsetKnown;
    int64_t stamp=0;
    if(valid) {
        auto dt=watchClock.value();
        stamp=coralliumtime::unixMs(dt.date.year,dt.date.month,dt.date.date,
            dt.time.hours,dt.time.minutes,dt.time.seconds,_offset);
        valid=coralliumtime::valid(stamp,_offset);
    }
    object["valid"]=valid; object["utc_offset_min"]=_offset;
    object["source"]=valid?"rtc":"unset"; object["quality"]=valid?"synchronized":"unset";
    if(valid) object["unix_ms"]=stamp;
    else object["unix_ms"]=nullptr;
}
void CoralliumDevice::status(JsonObject object) {
    timeStatus(object.createNestedObject("time"));
    // The common status shape permits unavailable channels. This device does
    // not advertise, initialize or configure Wi-Fi on the current SDK/hardware.
    auto wifi=object.createNestedObject("wifi"); wifi["state"]="disconnected";
    wifi["ssid"]=nullptr; wifi["ip"]=nullptr; wifi["rssi"]=nullptr;
    auto battery=object.createNestedObject("battery");
    if(_power->batteryMillivolts()) {
        battery["percent"]=_power->batteryPct(); battery["charging"]=_power->charging();
        battery["millivolts"]=_power->batteryMillivolts();
    } else { battery["percent"]=nullptr; battery["charging"]=nullptr; battery["millivolts"]=nullptr; }
    battery["power_mw"]=nullptr; battery["runtime_min"]=nullptr;
    object["uptime_ms"]=(uint64_t)(esp_timer_get_time()/1000);
}
void CoralliumDevice::send(JsonDocument& doc,bool serial) {
    if(serial) { serializeJson(doc,Serial); Serial.write('\n'); return; }
    if(!_encrypted||_outputLength) return;
    _outputLength=serializeJson(doc,_output,sizeof(_output)-1);
    _output[_outputLength++]='\n'; _outputOffset=0;
}
void CoralliumDevice::process(const char* line,bool serial) {
    DynamicJsonDocument request(3072);
    if(!coralliumvalidation::utf8(line)) return;
    coralliumvalidation::JsonInput reader{line};
    if(deserializeJson(request,reader)||!reader.atEnd()||!request.is<JsonObject>()) return;
    const char* id=request["id"],*op=request["op"];
    if(!validId(id)||!op||!op[0]||strlen(op)>64
        ||request["id"].as<JsonString>().size()!=strlen(id)
        ||request["op"].as<JsonString>().size()!=strlen(op)) return;
    DynamicJsonDocument response(2048);
    response["v"]=1; response["id"]=id; response["op"]=op;
    auto fail=[&](const char* code,const char* message) {
        response["ok"]=false; response.remove("payload");
        auto error=response.createNestedObject("error"); error["code"]=code; error["message"]=message;
        send(response,serial);
    };
    if(!request["v"].is<int>()||request["v"].as<int>()!=1||!request["payload"].is<JsonObject>()||request.containsKey("ok")) {
        fail("invalid_request","Invalid envelope"); return;
    }
    if(!serial&&!windowOpen()) { fail("not_authorized","Connection window closed"); return; }
    response["ok"]=true;
    auto output=response.createNestedObject("payload"); auto input=request["payload"].as<JsonObject>();
    if(!strcmp(op,"device.info")) {
        char deviceId[24]; snprintf(deviceId,sizeof(deviceId),"watch-%012llX",ESP.getEfuseMac());
        output["device_id"]=deviceId; output["model"]="m5stack-stopwatch";
        output["name"]="StopWatch"; output["firmware"]="bot-ux-watch/0.3.0";
        auto caps=output.createNestedArray("capabilities");
        caps.add("time.set"); caps.add("battery");
        auto channels=output.createNestedArray("channels"); channels.add("ble"); channels.add("serial");
    } else if(!strcmp(op,"device.status")) status(output);
    else if(!strcmp(op,"time.set")) {
        if(_clockEditing) { fail("busy","Local clock editor is open"); return; }
        if(!input["unix_ms"].is<int64_t>()||!input["utc_offset_min"].is<int>()) { fail("invalid_request","Invalid time"); return; }
        int64_t stamp=input["unix_ms"].as<int64_t>(); int offset=input["utc_offset_min"].as<int>();
        if(!coralliumtime::valid(stamp,offset)) { fail("invalid_request","Time outside RTC range"); return; }
        int64_t local=stamp/1000+offset*60;
        int days=local/86400,daySeconds=local%86400,year=1970,month=1;
        while(days>=(watchcalendar::daysInMonth(year,2)==29?366:365)) {
            days-=watchcalendar::daysInMonth(year,2)==29?366:365; ++year;
        }
        while(days>=watchcalendar::daysInMonth(year,month)) { days-=watchcalendar::daysInMonth(year,month); ++month; }
        m5::rtc_datetime_t dt(m5::rtc_date_t(year,month,days+1,0),
            m5::rtc_time_t(daySeconds/3600,(daySeconds/60)%60,daySeconds%60));
        Preferences prefs;
        if(!prefs.begin("corallium",false)) { fail("internal","Offset storage failed"); return; }
        auto saved=coralliumclock::synchronize(prefs,[&]() { return watchClock.set(dt); },
            (int16_t)offset,_offset,_offsetKnown);
        prefs.end();
        if(saved==coralliumclock::Save::StorageFailed) { fail("internal","Offset storage failed"); return; }
        if(saved==coralliumclock::Save::ClockFailed) { fail("internal","RTC write failed"); return; }
        timeStatus(output);
    } else { fail("unsupported","Unsupported operation"); return; }
    send(response,serial);
    if(!serial&&!strcmp(op,"device.info")&&_outputLength) _events.handshake(millis());
}
void CoralliumDevice::update(uint32_t now) {
    if(_disconnected) {
        _disconnected=false; _outputLength=_outputOffset=0; xQueueReset(_requests);
        if(windowOpen()) NimBLEDevice::startAdvertising();
    }
    if(_windowUntil&&!windowOpen()) closeWindow();
    // NimBLE 1.4 also resumes advertising after a failed connection, bypassing
    // advertiseOnDisconnect. The local window remains authoritative.
    if(_started&&!windowOpen()&&NimBLEDevice::getAdvertising()->isAdvertising())
        NimBLEDevice::stopAdvertising();
    if(_outputLength) {
        if(!_encrypted) { _outputLength=_outputOffset=0; }
        else if(_tx->getSubscribedCount()&&now-_lastNotify>=8) {
            _lastNotify=now; size_t bytes=min((size_t)20,_outputLength-_outputOffset);
            auto* buffer=ble_hs_mbuf_from_flat(_output+_outputOffset,bytes);
            if(buffer&&ble_gattc_notify_custom(_connection,_tx->getHandle(),buffer)==0) {
                _outputOffset+=bytes;
                if(_outputOffset==_outputLength) { memset(_output,0,sizeof(_output)); _outputLength=_outputOffset=0; }
            }
        }
    }
    if(!_outputLength&&_requests) {
        if(xQueueReceive(_requests,&_pending,0)==pdTRUE) {
            process(_pending.line,false);
            memset(_pending.line,0,sizeof(_pending.line));
        }
        else if(_encrypted&&_events.due(now)) {
            DynamicJsonDocument event(2048);
            event["v"]=1; event["op"]="device.status"; status(event.createNestedObject("payload")); send(event,false);
        }
    }
}
