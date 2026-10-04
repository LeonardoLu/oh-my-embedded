#pragma once
#include "UxSound.h"

namespace ux { namespace sound {
enum class PcmEncoding : uint8_t { Signed8, Unsigned8 };
struct PcmClip {
    const void* data = nullptr;
    size_t samples = 0; // mono samples, not WAV file bytes
    uint32_t sampleRate = SampleRate;
    PcmEncoding encoding = PcmEncoding::Unsigned8;
};
// Optional object: no PCM decoder/assets are linked by Synth-only consumers.
// Clip storage is borrowed and must remain valid until playback/preemption ends.
// For rates above 16k, prepare band-limited input offline (this is not a codec).
class PcmPlayer : public Source {
public:
    bool play(PcmClip clip, Priority priority = Priority::Notification);
    void setEnabled(bool enabled);
    void setVolume(uint8_t volume) { _volume = volume; if (!active()) _gain = volume / 255.0f; }
    uint8_t volume() const { return _volume; }
    bool enabled() const { return _enabled; }
    void cancel() override;
    bool active() const override { return _clip.data != nullptr; }
    size_t render(int16_t* output, size_t capacity) override;
private:
    void start(PcmClip clip, Priority priority);
    PcmClip _clip{}, _pending{};
    Priority _priority = Priority::Ambient, _pendingPriority = Priority::Ambient;
    uint64_t _cursor = 0, _step = 0;
    uint32_t _position = 0, _duration = 0;
    uint16_t _releaseLeft = 0;
    uint8_t _volume = 180;
    bool _enabled = true;
    float _gain = 180.0f / 255;
};
} }
