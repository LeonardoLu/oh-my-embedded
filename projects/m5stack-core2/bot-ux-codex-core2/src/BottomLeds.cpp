#include "BottomLeds.h"

#include <M5Unified.h>
#include <utility/led/LED_Strip_Class.hpp>
#include <memory>

namespace {
constexpr uint8_t kCount = 10;
constexpr int8_t kDataPin = 25;
// LED_Strip_Class squares (driverBrightness + 1) before scaling by 65536.
// These inverse-curve levels produce full-channel outputs 0, 16, 38, 71,
// approximating the intended linear user levels 0, 16, 38, 72 out of 255.
constexpr uint8_t kBrightness[4] = {0, 64, 98, 135};

#if M5UNIFIED_RMT_VERSION == 1
// M5Unified 0.2.21 leaves its IDF4 LedBus_RMT init/write branches empty.
// Supply the legacy peripheral transport while retaining LED_Strip_Class's
// GRB packing, brightness curve, storage, and public M5.Led integration.
class Bottom2LegacyRmtBus : public m5::LedBus_Base {
public:
    ~Bottom2LegacyRmtBus() override { release(); }
    const m5::LedBus_RMT::config_t& getConfig() const { return _config; }
    void setConfig(const m5::LedBus_RMT::config_t& config) { _config = config; }

    bool init() override
    {
        if (_installed) return true;
        rmt_channel_status_result_t status{};
        esp_err_t error = rmt_get_channel_status(&status);
        if (error != ESP_OK) {
            Serial.printf("[bottom2] RMT status failed: %s\n", esp_err_to_name(error));
            return false;
        }
        bool reserved[RMT_CHANNEL_MAX]{};
        for (uint8_t index = 0; index < RMT_CHANNEL_MAX; ++index)
        {
            if (status.status[index] == RMT_CHANNEL_UNINIT) continue;
            uint8_t blocks = 1;
            rmt_get_mem_block_num(static_cast<rmt_channel_t>(index), &blocks);
            for (uint8_t block = 0; block < blocks && index + block < RMT_CHANNEL_MAX; ++block)
                reserved[index + block] = true;
        }
        m5gfx::gpio_lo(_config.pin_data);
        m5gfx::pinMode(_config.pin_data, m5gfx::pin_mode_t::output);
        error = ESP_ERR_NOT_FOUND;
        for (uint8_t index = 0; index < RMT_CHANNEL_MAX; ++index)
        {
            if (reserved[index]) continue;
            const auto channel = static_cast<rmt_channel_t>(index);
            rmt_config_t config{};
            config.rmt_mode = RMT_MODE_TX;
            config.channel = channel;
            config.gpio_num = static_cast<gpio_num_t>(_config.pin_data);
            config.clk_div = 8; // 80 MHz APB / 8 = 100 ns per tick.
            config.mem_block_num = 1;
            config.tx_config.loop_en = false;
            config.tx_config.carrier_en = false;
            config.tx_config.idle_output_en = true;
            config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
            error = rmt_config(&config);
            if (error != ESP_OK) continue;
            error = rmt_driver_install(channel, 0, 0);
            if (error != ESP_OK)
            {
                // IDF4 can retain its allocated channel object when ISR
                // registration fails. Never uninstall an already-owned channel.
                if (error != ESP_ERR_INVALID_STATE) rmt_driver_uninstall(channel);
                continue;
            }
            _channel = channel;
            _installed = true;
            Serial.printf("[bottom2] legacy RMT ready gpio=%d channel=%u leds=%u\n",
                          _config.pin_data, index, kCount);
            return true;
        }
        Serial.printf("[bottom2] no usable RMT channel: %s\n", esp_err_to_name(error));
        return false;
    }

    void release() override
    {
        if (!_installed) return;
        rmt_driver_uninstall(_channel);
        _installed = false;
    }

    void write(const uint8_t* data, size_t length) override
    {
        if (!_installed || !data || length > kCount * 3) return;
        size_t count = 0;
        for (size_t byte = 0; byte < length; ++byte)
        {
            for (uint8_t mask = 0x80; mask; mask >>= 1)
            {
                const bool one = (data[byte] & mask) != 0;
                rmt_item32_t& item = _items[count++];
                item.level0 = 1;
                item.duration0 = (one ? _config.t1h_ns : _config.t0h_ns) / 100;
                item.level1 = 0;
                item.duration1 = (one ? _config.t1l_ns : _config.t0l_ns) / 100;
            }
        }
        rmt_item32_t& reset = _items[count++];
        reset.level0 = 0;
        reset.duration0 = _config.reset_us * 5;
        reset.level1 = 0;
        reset.duration1 = _config.reset_us * 5;
        // A complete ten-pixel frame and latch takes under 0.6 ms. Waiting
        // keeps this fixed buffer alive and unchanged until peripheral/ISR use ends.
        const esp_err_t error = rmt_write_items(_channel, _items, count, true);
        if (error != ESP_OK && !_writeErrorReported)
            Serial.printf("[bottom2] RMT write failed: %s\n", esp_err_to_name(error));
        _writeErrorReported = error != ESP_OK;
    }

private:
    m5::LedBus_RMT::config_t _config;
    rmt_channel_t _channel = RMT_CHANNEL_0;
    rmt_item32_t _items[kCount * 3 * 8 + 1]{};
    bool _installed = false;
    bool _writeErrorReported = false;
};
#endif
}

bool BottomLeds::begin()
{
#if M5UNIFIED_RMT_VERSION == 1
    auto bus = std::make_shared<Bottom2LegacyRmtBus>();
#else
    auto bus = std::make_shared<m5::LedBus_RMT>();
#endif
    auto busConfig = bus->getConfig();
    busConfig.pin_data = kDataPin;
    bus->setConfig(busConfig);

    auto strip = std::make_shared<m5::LED_Strip_Class>();
    auto stripConfig = strip->getConfig();
    stripConfig.led_count = kCount;
    stripConfig.byte_per_led = 3;
    stripConfig.color_order = m5::LED_Strip_Class::config_t::color_order_grb;
    strip->setBus(bus);
    strip->setConfig(stripConfig);
    M5.Led.setLedInstance(strip);
    M5.Led.setAutoDisplay(false);
    _available = M5.Led.begin();
    setBrightness(_brightness);
    if (_available)
    {
        M5.Led.setAllColor(0, 0, 0);
        M5.Led.display();
    }
    return _available;
}

void BottomLeds::setBrightness(uint8_t level)
{
    _brightness = level > 3 ? 2 : level;
    if (_available)
    {
        M5.Led.setBrightness(kBrightness[_brightness]);
        if (_brightness == 0)
        {
            for (auto& color : _output) color = bottomled::Color{};
            M5.Led.setAllColor(0, 0, 0);
            M5.Led.display();
        }
    }
}

void BottomLeds::setMode(uint8_t mode)
{
    const uint8_t normalized = mode <= 2 ? mode : 2;
    if (_mode == normalized) return;
    _mode = normalized;
    if (_mode == 0 && _available)
    {
        for (auto& color : _output) color = bottomled::Color{};
        M5.Led.setAllColor(0, 0, 0);
        M5.Led.display();
    }
}

void BottomLeds::interact(uint8_t selected, uint32_t nowMs)
{
    _events.interactionSlot = selected < LightingState::kSlotCount ? selected : 0;
    _events.interactionAt = nowMs;
    _events.hasInteraction = true;
}

void BottomLeds::notify(uint8_t mask, uint32_t nowMs)
{
    _events.notificationMask = mask & 0x3Fu;
    _events.notificationAt = nowMs;
}

void BottomLeds::control(uint32_t nowMs)
{
    _events.controlAt = nowMs;
    _events.hasControl = true;
}

void BottomLeds::hold(bool active, uint32_t nowMs)
{
    if (active)
    {
        _events.controlHeld = true;
        return;
    }
    if (!_events.controlHeld) return;
    _events.controlHeld = false;
    control(nowMs);
}

void BottomLeds::update(const LightingState& lighting, uint32_t nowMs, bool reducedMotion,
                        bool ready, uint8_t selectedAgent, uint8_t freshReplyMask,
                        uint8_t theme)
{
    if (!_available || nowMs - _lastFrameMs < 50) return;
    _lastFrameMs = nowMs;
    _events.freshReplyMask = freshReplyMask & 0x3Fu;

    bottomled::Color generated[kCount]{};
    bottomled::frame(lighting, _events, nowMs, reducedMotion, ready,
                     selectedAgent, theme, _mode, _brightness != 0, generated);
    RGBColor colors[kCount]{};
    for (uint8_t i = 0; i < kCount; ++i)
    {
        _output[i] = generated[i];
        colors[i] = RGBColor{generated[i].r, generated[i].g, generated[i].b};
    }
    M5.Led.setColors(colors, 0, kCount);
    M5.Led.display();
}
