#pragma once
#include "UxSound.h"
#include <atomic>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace ux { namespace sound {
// Keep this output and its speaker/source alive for the lifetime of the device.
// Only the host calls update()/Source methods. Only the sender calls playRaw().
// SDK queue publication can wait indefinitely, so it must never run on the UI.
template<class Speaker> class M5Output {
public:
    enum class PowerState : uint8_t { Ready, Suspending, Suspended, Resuming, Failed };
    M5Output() = default;
    M5Output(const M5Output&) = delete;
    M5Output& operator=(const M5Output&) = delete;
    static constexpr size_t ChunkSamples = 2048; // 128 ms at 16 kHz
    bool begin(Speaker& speaker, Source& source, uint8_t channel = 6, bool active = true) {
        if (_speaker || channel >= 8) return false;
        // Initial active startup remains synchronous. No sender exists yet, so
        // this cannot race playRaw; every later begin/end belongs to the sender.
        if (active && (!speaker.begin() || !speaker.isRunning() || !speaker.isEnabled())) {
            speaker.end();
            return false;
        }
        _speaker = &speaker; _source = &source; _channel = channel;
        _desiredReady.store(active, std::memory_order_release);
        _powerState.store(active ? PowerState::Ready : PowerState::Suspending,
                          std::memory_order_release);
        if (active) {
            _resumeRequest.store(1, std::memory_order_release);
            _readyRequest.store(1, std::memory_order_release);
            _hardwareStarted.store(true, std::memory_order_release);
            _handledResumeRequest = 1;
        } else {
            source.cancel();
            _producerDrained.store(!source.active(), std::memory_order_release);
        }
#if defined(ESP_PLATFORM)
        // ESP-IDF task stack sizes are bytes. This task never synthesizes audio.
        if (xTaskCreate(_taskEntry, "ux-audio-send", 2048, this, 1, &_sender) != pdPASS) {
            speaker.end();
            _desiredReady.store(false, std::memory_order_release);
            _hardwareStarted.store(false, std::memory_order_release);
            _powerState.store(PowerState::Suspended, std::memory_order_release);
            _speaker = nullptr; _source = nullptr; return false;
        }
#endif
        return true;
    }
    bool setSource(Source& source) {
        if (!_speaker || busy()) return false;
        _source = &source; return true;
    }
    // Bounded synthesis only: no Speaker call, queue wait, or frame-time heap.
    // Call every host loop, at most 100 ms apart during normal streaming.
    void update() {
        if (!_speaker || !_source) return;
        if (!_desiredReady.load(std::memory_order_acquire)) {
            _updateSuspendingSource();
#if defined(ESP_PLATFORM)
            xTaskNotifyGive(_sender);
#endif
            return;
        }
        if (!ready()) return;
        _produce();
    }
    // Lifecycle requests never call the SDK. The sender acknowledges only after
    // begin/end returns, so suspended() is the gate for CPU light sleep.
    bool suspend() {
        if (!_speaker) return false;
        if (!_desiredReady.load(std::memory_order_acquire)) return true;
        const bool drainToHardware = _hardwareStarted.load(std::memory_order_acquire)
                                  && _powerState.load(std::memory_order_acquire) == PowerState::Ready;
        _drainToHardware.store(drainToHardware, std::memory_order_release);
        // Publish a closed drain gate before the off request. The sender can
        // never observe desired=false together with an acknowledgement from a
        // previous suspend generation.
        _producerDrained.store(false, std::memory_order_release);
        _source->cancel();
        _producerDrained.store(!_source->active(), std::memory_order_release);
        _desiredReady.store(false, std::memory_order_release);
#if defined(ESP_PLATFORM)
        xTaskNotifyGive(_sender);
#endif
        return true;
    }
    bool resume() {
        if (!_speaker) return false;
        const bool wasDesired = _desiredReady.load(std::memory_order_acquire);
        if (!wasDesired || _powerState.load(std::memory_order_acquire) == PowerState::Failed) {
            _resumeRequest.fetch_add(1, std::memory_order_release);
            _desiredReady.store(true, std::memory_order_release);
#if defined(ESP_PLATFORM)
            xTaskNotifyGive(_sender);
#endif
        }
        return true;
    }
    PowerState powerState() const { return _powerState.load(std::memory_order_acquire); }
    bool ready() const {
        return _desiredReady.load(std::memory_order_acquire)
            && powerState() == PowerState::Ready
            && _readyRequest.load(std::memory_order_acquire)
                == _resumeRequest.load(std::memory_order_acquire);
    }
    bool suspended() const {
        return !_desiredReady.load(std::memory_order_acquire)
            && powerState() == PowerState::Suspended;
    }
    bool failed() const { return powerState() == PowerState::Failed; }
    bool desiredReady() const { return _desiredReady.load(std::memory_order_acquire); }
    bool busy() const { return _source && (_source->active() || _inFlight.load(std::memory_order_acquire)); }
    void cancel() { if (_source) _source->cancel(); }
    uint32_t failures() const { return _failures.load(std::memory_order_relaxed); }
#if !defined(ESP_PLATFORM)
    // Host tests run this on their simulated sender, never on the UI producer.
    void transportStep() { _serviceTransport(); }
#endif
private:
    enum SlotState : uint8_t { Free, ReadySlot, Submitted };
    struct Slot {
        std::atomic<uint8_t> state{Free};
        size_t length = 0;
        int16_t samples[ChunkSamples]{};
    };
    void _produce() {
        for (unsigned generated = 0; generated < 2; ++generated) {
            if (_inFlight.load(std::memory_order_acquire) >= 2 || !_source->active()) break;
            Slot& slot = _slots[_produceSequence % 3];
            if (slot.state.load(std::memory_order_acquire) != Free) break;
            slot.length = _source->render(slot.samples, ChunkSamples);
            if (!slot.length) break;
            // The worker can retire old buffers concurrently, but the sole
            // producer is the only task that increases the ahead count.
            _inFlight.fetch_add(1, std::memory_order_release);
            slot.state.store(ReadySlot, std::memory_order_release);
            ++_produceSequence;
#if defined(ESP_PLATFORM)
            xTaskNotifyGive(_sender);
#endif
        }
    }
    void _updateSuspendingSource() {
        // Once the original source has drained, later rejected/stale cues are
        // consumed locally and can neither delay shutdown nor reopen hardware.
        if (_producerDrained.load(std::memory_order_acquire)) {
            if (!_source->active()) return;
            _source->cancel();
            for (unsigned i = 0; i < 2 && _source->active(); ++i)
                _source->render(_discard, DiscardSamples);
            return;
        }
        if (_source->active()) {
            if (_drainToHardware.load(std::memory_order_acquire)
                    && _hardwareStarted.load(std::memory_order_acquire)) {
                _produce();
            } else {
                for (unsigned i = 0; i < 2 && _source->active(); ++i)
                    _source->render(_discard, DiscardSamples);
            }
        }
        if (!_source->active()) {
            _drainToHardware.store(false, std::memory_order_release);
            _producerDrained.store(true, std::memory_order_release);
        }
    }
    void _retireCompleted() {
        // No other producer may use this channel. The count includes published
        // AND playing slots. A decrease retires successful submissions in FIFO
        // order; an in-progress playRaw is never retired until it returns.
        const size_t occupied = _speaker->isPlaying(_channel);
        // SDK slot retirement is a release store, but isPlaying reads relaxed.
        // Pair those observed retirements before publishing our buffers Free.
        std::atomic_thread_fence(std::memory_order_acquire);
        while (_submittedCount > occupied) {
            const uint8_t index = _submitted[_submittedHead];
            _submittedHead = (_submittedHead + 1) % 2;
            --_submittedCount;
            _slots[index].state.store(Free, std::memory_order_release);
            _inFlight.fetch_sub(1, std::memory_order_release);
        }
    }
    void _serviceTransport() {
        if (!_speaker) return;
        bool wantReady = _desiredReady.load(std::memory_order_acquire);
        if (!_hardwareStarted.load(std::memory_order_acquire)) {
            if (!wantReady) {
                if (_speaker->isRunning()) {
                    _hardwareStarted.store(true, std::memory_order_release);
                } else if (_producerDrained.load(std::memory_order_acquire)) {
                    _powerState.store(PowerState::Suspended, std::memory_order_release);
                    return;
                } else {
                    _powerState.store(PowerState::Suspending, std::memory_order_release);
                    return;
                }
            } else {
                const uint32_t request = _resumeRequest.load(std::memory_order_acquire);
                if (_powerState.load(std::memory_order_acquire) == PowerState::Failed
                        && request == _handledResumeRequest) return;
                _powerState.store(PowerState::Resuming, std::memory_order_release);
                bool started = _speaker->isRunning() && _speaker->isEnabled();
                if (!started) started = _speaker->begin()
                                     && _speaker->isRunning() && _speaker->isEnabled();
                _handledResumeRequest = request;
                if (!started) {
                    _speaker->end();
                    _failures.fetch_add(1, std::memory_order_relaxed);
                    _hardwareStarted.store(false, std::memory_order_release);
                    _powerState.store(_desiredReady.load(std::memory_order_acquire)
                                      ? PowerState::Failed : PowerState::Suspended,
                                      std::memory_order_release);
                    return;
                }
                _hardwareStarted.store(true, std::memory_order_release);
            }
        }
        wantReady = _desiredReady.load(std::memory_order_acquire);
        _powerState.store(wantReady ? PowerState::Ready : PowerState::Suspending,
                          std::memory_order_release);
        if (wantReady)
            _readyRequest.store(_resumeRequest.load(std::memory_order_acquire),
                                std::memory_order_release);
        _retireCompleted();
        for (unsigned sent = 0; sent < 2 && _submittedCount < 2; ++sent) {
            const uint8_t index = _sendSequence % 3;
            Slot& slot = _slots[index];
            uint8_t ready = ReadySlot;
            if (!slot.state.compare_exchange_strong(ready, Submitted, std::memory_order_acquire)) break;
            // isPlaying()<2 does NOT prove flip is writable: one published
            // request may still await adoption. Only this worker may wait here.
            if (!_speaker->playRaw(slot.samples, slot.length, SampleRate, false, 1, _channel, false)) {
                _failures.fetch_add(1, std::memory_order_relaxed);
                slot.state.store(ReadySlot, std::memory_order_release);
                break; // retry identical samples; do not advance Source again
            }
            _submitted[(_submittedHead + _submittedCount) % 2] = index;
            ++_submittedCount;
            ++_sendSequence;
            _retireCompleted();
        }
        if (!_desiredReady.load(std::memory_order_acquire)) {
            // Publish the non-ready state before the final decision to close.
            // A resume racing a slow end invalidates the old ready generation,
            // so the host cannot produce into hardware being torn down.
            _powerState.store(PowerState::Suspending, std::memory_order_release);
            if (_producerDrained.load(std::memory_order_acquire)
                    && !_inFlight.load(std::memory_order_acquire)
                    && !_desiredReady.load(std::memory_order_acquire)) {
                _speaker->end();
                _hardwareStarted.store(false, std::memory_order_release);
                if (_desiredReady.load(std::memory_order_acquire)) {
                    _powerState.store(PowerState::Resuming, std::memory_order_release);
                } else {
                    _powerState.store(PowerState::Suspended, std::memory_order_release);
                }
            }
        }
    }
#if defined(ESP_PLATFORM)
    static void _taskEntry(void* context) {
        auto* self = static_cast<M5Output*>(context);
        for (;;) {
            self->_serviceTransport();
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1);
        }
    }
    TaskHandle_t _sender = nullptr;
#endif
    Speaker* _speaker = nullptr;
    Source* _source = nullptr;
    uint8_t _channel = 6;
    uint32_t _produceSequence = 0; // host-owned
    uint32_t _sendSequence = 0;    // sender-owned
    uint8_t _submitted[2]{}, _submittedHead = 0, _submittedCount = 0;
    std::atomic<uint8_t> _inFlight{0};
    std::atomic<uint32_t> _failures{0};
    std::atomic<bool> _desiredReady{false};
    std::atomic<bool> _hardwareStarted{false};
    std::atomic<bool> _producerDrained{true};
    std::atomic<bool> _drainToHardware{false};
    std::atomic<PowerState> _powerState{PowerState::Suspended};
    std::atomic<uint32_t> _resumeRequest{0};
    std::atomic<uint32_t> _readyRequest{0};
    uint32_t _handledResumeRequest = 0; // sender-owned
    static constexpr size_t DiscardSamples = 128;
    int16_t _discard[DiscardSamples]{}; // host-owned cancellation sink
    Slot _slots[3]{};
};
} }
