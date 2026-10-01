#include "Power.h"
#include "PowerPolicy.h"
#include <M5Unified.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

void Power::begin() {
    // Disable only single-click reset; leave double-off and download intact.
    uint8_t cfg=0, verify=0;
    auto& pm=M5.Power.M5pm1;
    pm.readRegister(0x4a,&_bootOffCfg,1);
    bool readKey=pm.readRegister(0x49,&cfg,1); _bootKeyCfg=cfg;
    _homeKeyReady=readKey
        && pm.writeRegister8(0x49,cfg|0x01)
        && pm.readRegister(0x49,&verify,1) && verify==(uint8_t)(cfg|0x01);
    pm.clearButtonIRQStatus();
    setIndicator(false);
    applyLevel(3);
    update();
}

void Power::update() {
    _rtcHoldReady=watchpower::retainRtcPower(M5.Power.M5pm1);
    const int mv = M5.Power.getBatteryVoltage();
    if (mv >= 3000 && mv <= 4400) {
        _filteredMv = (_filteredMv == 0)
            ? (uint16_t)mv
            : (uint16_t)((_filteredMv * 7U + (uint16_t)mv) / 8U);
        int pct = ((int)_filteredMv - 3300) * 100 / 900;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        _batteryPct = (uint8_t)pct;
    }

    // M5Unified reads M5PM1 GPIO2 active-low for StopWatch. Gate it with VIN
    // so a floating/status-low line cannot claim charging when unplugged.
    const int vinMv = M5.Power.getVBUSVoltage();
    _externalPower = vinMv > 4000;
    _charging = _externalPower
        && M5.Power.isCharging() == m5::Power_Class::is_charging;
}

bool Power::readPowerButton(bool* pressed) {
    return M5.Power.M5pm1.getButtonPressed(pressed);
}

uint8_t Power::levelToValue(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    static const uint8_t kMap[5] = { 32, 72, 120, 180, 245 };
    return kMap[level - 1];
}

void Power::setBrightness(uint8_t v) {
    M5.Display.setBrightness(v);
}

void Power::applyLevel(uint8_t level) {
    setBrightness(levelToValue(level));
}

void Power::sleepDisplay() {
    if (_displaySleeping) return;
    M5.Display.sleep();
    _displaySleeping = true;
}

void Power::wakeDisplay(uint8_t level) {
    if (_displaySleeping) M5.Display.wakeup();
    _displaySleeping = false;
    applyLevel(level);
}

bool Power::lightSleepForButtonPoll(uint64_t fallbackUs) {
    // StopWatch A/B are direct active-low ESP32-S3 GPIO2/GPIO1. The timer
    // keeps the PMIC power key and VIN observable without changing PM1 IRQ
    // routing, so a failed or board-revision-specific IRQ cannot strand it.
    pinMode(1, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);
    if (digitalRead(1) == LOW || digitalRead(2) == LOW) return true;

    const uint64_t buttons = (1ULL << 1) | (1ULL << 2);
    if (esp_sleep_enable_ext1_wakeup(buttons, ESP_EXT1_WAKEUP_ANY_LOW) != ESP_OK
        || esp_sleep_enable_timer_wakeup(fallbackUs) != ESP_OK) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        delay(8);
        return false;
    }
    esp_err_t result=esp_light_sleep_start();
    bool buttonWake=result==ESP_OK
        &&(esp_sleep_get_ext1_wakeup_status()&buttons)!=0;
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    rtc_gpio_hold_dis(GPIO_NUM_1);
    rtc_gpio_hold_dis(GPIO_NUM_2);
    rtc_gpio_deinit(GPIO_NUM_1);
    rtc_gpio_deinit(GPIO_NUM_2);
    pinMode(1, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);
    if(result!=ESP_OK) delay(8);
    return buttonWake;
}

bool Power::setIndicator(bool enabled) {
    if (_indicatorReady && _indicator==enabled) return true;
    uint8_t cfg=0;
    auto& pm=M5.Power.M5pm1;
    _indicatorReady=pm.setLedEnLevel(enabled) && pm.readRegister(0x06,&cfg,1)
        && ((cfg&0x10)!=0)==enabled;
    if (_indicatorReady) _indicator=enabled;
    return _indicatorReady;
}
