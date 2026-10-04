#include "UxSoundPcm.h"
namespace ux { namespace sound {
namespace {
constexpr uint16_t ReleaseSamples = SampleRate * 8 / 1000;
int32_t read(PcmClip clip, size_t i) {
    return clip.encoding == PcmEncoding::Signed8
        ? static_cast<const int8_t*>(clip.data)[i]
        : static_cast<int32_t>(static_cast<const uint8_t*>(clip.data)[i]) - 128;
}
float fraction(uint32_t n, uint32_t d) { return n >= d ? 1.0f : static_cast<float>(n) / d; }
}
bool PcmPlayer::play(PcmClip clip, Priority priority) {
    if (!_enabled || !clip.data || !clip.samples || clip.samples > 16000000
        || clip.sampleRate < 4000 || clip.sampleRate > 48000
        || static_cast<unsigned>(clip.encoding) > 1) return false;
    Priority occupied = _pending.data ? _pendingPriority : _priority;
    if (active() && static_cast<unsigned>(priority) < static_cast<unsigned>(occupied)) return false;
    if (!active()) { start(clip, priority); return true; }
    _pending = clip; _pendingPriority = priority;
    if (!_releaseLeft) _releaseLeft = ReleaseSamples;
    return true;
}
void PcmPlayer::start(PcmClip clip, Priority priority) {
    _clip = clip; _priority = priority; _cursor = _position = _releaseLeft = 0;
    _step = (static_cast<uint64_t>(clip.sampleRate) << 16) / SampleRate;
    _duration = (static_cast<uint64_t>(clip.samples) * SampleRate + clip.sampleRate - 1) / clip.sampleRate;
}
void PcmPlayer::cancel() { _pending.data = nullptr; if (active() && !_releaseLeft) _releaseLeft = ReleaseSamples; }
void PcmPlayer::setEnabled(bool enabled) { _enabled = enabled; if (!enabled) cancel(); }
size_t PcmPlayer::render(int16_t* output, size_t capacity) {
    if (!output) return 0;
    size_t count = 0;
    while (count < capacity && active()) {
        size_t i = _cursor >> 16;
        if (i >= _clip.samples) i = _clip.samples - 1;
        int32_t a = read(_clip, i), b = read(_clip, i + 1 < _clip.samples ? i + 1 : i);
        float wave = a + (b - a) * static_cast<float>(_cursor & 65535) / 65536;
        uint32_t fade = _duration < 288 ? _duration / 3 : 96;
        if (!fade) fade = 1;
        float attack = fraction(_position, fade), release = fraction(_duration - 1 - _position, fade);
        float envelope = attack * attack * (3 - 2 * attack) * release * release * (3 - 2 * release);
        if (_releaseLeft) envelope *= static_cast<float>(_releaseLeft - 1) / ReleaseSamples;
        _gain += (_volume / 255.0f - _gain) / 64;
        output[count++] = static_cast<int16_t>(wave * (15000.0f / 128) * envelope * _gain);
        _cursor += _step; ++_position;
        bool released = _releaseLeft && --_releaseLeft == 0;
        if (released || _position >= _duration) {
            _clip.data = nullptr;
            if (_pending.data) { PcmClip next = _pending; _pending.data = nullptr; start(next, _pendingPriority); }
        }
    }
    for (size_t i = count; i < capacity; ++i) output[i] = 0;
    return count;
}
} }
