#include "UxSound.h"
#include <math.h>
#include <string.h>

namespace ux { namespace sound {
namespace {
constexpr uint16_t ReleaseSamples = SampleRate * 8 / 1000;
constexpr int16_t Peak = 15000; // headroom before M5's mixer and hardware gain
const int16_t sineTable[256] = {
    0,804,1608,2410,3212,4011,4808,5602,6393,7179,7962,8739,9512,10278,11039,11793,
    12539,13279,14010,14732,15446,16151,16846,17530,18204,18868,19519,20159,20787,21403,22005,22594,
    23170,23731,24279,24811,25329,25832,26319,26790,27245,27683,28105,28510,28898,29268,29621,29956,
    30273,30571,30852,31113,31356,31580,31785,31971,32137,32285,32412,32521,32609,32678,32728,32757,
    32767,32757,32728,32678,32609,32521,32412,32285,32137,31971,31785,31580,31356,31113,30852,30571,
    30273,29956,29621,29268,28898,28510,28105,27683,27245,26790,26319,25832,25329,24811,24279,23731,
    23170,22594,22005,21403,20787,20159,19519,18868,18204,17530,16846,16151,15446,14732,14010,13279,
    12539,11793,11039,10278,9512,8739,7962,7179,6393,5602,4808,4011,3212,2410,1608,804,
    0,-804,-1608,-2410,-3212,-4011,-4808,-5602,-6393,-7179,-7962,-8739,-9512,-10278,-11039,-11793,
    -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,-18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
    -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,-27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
    -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,-32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
    -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,-32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
    -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,-27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
    -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,-18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
    -12539,-11793,-11039,-10278,-9512,-8739,-7962,-7179,-6393,-5602,-4808,-4011,-3212,-2410,-1608,-804,
};
int32_t sine(uint32_t phase) {
    unsigned i = phase >> 24;
    int32_t a = sineTable[i], b = sineTable[(i + 1) & 255];
    return a + ((b - a) * static_cast<int32_t>((phase >> 8) & 65535)) / 65536;
}
float limit(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
Note note(uint8_t midi, uint16_t ms, Timbre timbre, uint8_t velocity = 180,
          uint16_t gap = 8, int8_t glide = 0) {
    Note n; n.midi = midi; n.durationMs = ms; n.timbre = timbre;
    n.velocity = velocity; n.gapMs = gap; n.glideSemitones = glide; return n;
}
Priority cuePriority(Cue cue) {
    if (cue == Cue::Error || cue == Cue::Warning || cue == Cue::Reject) return Priority::Alert;
    if (cue == Cue::Connected || cue == Cue::Disconnected || cue == Cue::Success) return Priority::Notification;
    if (cue == Cue::Sleep) return Priority::Ambient;
    return Priority::Interaction;
}
}

float midiFrequency(uint8_t midi) {
    static const float semitones[] = {16.3515978f,17.3239144f,18.3540480f,19.4454365f,
        20.6017223f,21.8267645f,23.1246514f,24.4997147f,25.9565436f,27.5f,29.1352351f,30.8677063f};
    return ldexpf(semitones[midi % 12], static_cast<int>(midi / 12) - 1);
}

uint8_t scaleMidi(uint8_t root, uint8_t degree, Scale scale) {
    static const uint8_t major[] = {0,2,4,5,7,9,11}, minor[] = {0,2,3,5,7,8,10}, pentatonic[] = {0,2,4,7,9};
    unsigned count = scale == Scale::Pentatonic ? 5 : 7;
    const uint8_t* intervals = scale == Scale::Major ? major : scale == Scale::Minor ? minor : pentatonic;
    unsigned result = root + 12 * (degree / count) + intervals[degree % count];
    return result > 127 ? 127 : result;
}

bool Synth::play(Cue cue) { return play(cue, cuePriority(cue)); }

bool Synth::play(Cue cue, Priority priority) {
    Note n[MaxNotes]; size_t count = 0;
    switch (cue) {
        case Cue::Tap: n[count++] = note(84, 28, Timbre::Pluck, 130, 0); break;
        case Cue::Select: n[count++] = note(79, 45, Timbre::Bell, 150, 0, 2); break;
        case Cue::Action: n[count++] = note(72, 45, Timbre::SoftSquare, 160); n[count++] = note(79, 60, Timbre::Pluck); break;
        case Cue::Confirm: n[count++] = note(76, 60, Timbre::Bell); n[count++] = note(83, 100, Timbre::Chime); break;
        case Cue::Back: n[count++] = note(76, 70, Timbre::Sine, 150, 0, -5); break;
        case Cue::Open: n[count++] = note(67, 65, Timbre::Pluck); n[count++] = note(74, 85, Timbre::Bell); break;
        case Cue::Close: n[count++] = note(74, 55, Timbre::Bell, 155); n[count++] = note(67, 65, Timbre::Pluck, 145); break;
        case Cue::Success: n[count++] = note(72, 70, Timbre::Chime); n[count++] = note(76, 70, Timbre::Chime); n[count++] = note(79, 150, Timbre::Bell, 200); break;
        case Cue::Warning: n[count++] = note(69, 100, Timbre::SoftSquare, 160, 45); n[count++] = note(69, 110, Timbre::Bell, 165); break;
        case Cue::Error: n[count++] = note(58, 100, Timbre::SoftSquare, 160, 18); n[count++] = note(53, 150, Timbre::Pluck, 170); break;
        case Cue::Reject: n[count++] = note(64, 100, Timbre::SoftSquare, 160, 0, -7); break;
        case Cue::Connected: n[count++] = note(72, 65, Timbre::Bell); n[count++] = note(79, 65, Timbre::Bell); n[count++] = note(84, 135, Timbre::Chime); break;
        case Cue::Disconnected: n[count++] = note(79, 85, Timbre::Bell, 155); n[count++] = note(72, 120, Timbre::Sine, 145); break;
        case Cue::Wake: n[count++] = note(60, 100, Timbre::Sine, 140, 4, 7); n[count++] = note(79, 170, Timbre::Chime, 170); break;
        case Cue::Sleep: n[count++] = note(72, 120, Timbre::Sine, 120); n[count++] = note(67, 150, Timbre::Sine, 110); n[count++] = note(60, 190, Timbre::Bell, 90); break;
        case Cue::Poke: n[count++] = note(72, 70, Timbre::Pluck, 170, 0, 12); n[count++] = note(84, 90, Timbre::Bell, 160, 0, -5); break;
        default: return false;
    }
    n[count - 1].gapMs = 0;
    return play(n, count, priority);
}

bool Synth::play(const Note* notes, size_t count, Priority priority) {
    if (!_enabled || !notes || !count || count > MaxNotes) return false;
    for (size_t i = 0; i < count; ++i)
        if (notes[i].midi > 108 || notes[i].durationMs < 10 || notes[i].durationMs > 2000
            || notes[i].gapMs > 1000 || static_cast<unsigned>(notes[i].timbre) > static_cast<unsigned>(Timbre::Noise)) return false;
    Priority occupied = _pendingCount ? _pendingPriority : _priority;
    if (active() && static_cast<unsigned>(priority) < static_cast<unsigned>(occupied)) return false;
    if (!active()) { start(notes, count, priority); return true; }
    memcpy(_pending, notes, count * sizeof(Note)); _pendingCount = count; _pendingPriority = priority;
    if (!_releaseLeft) _releaseLeft = ReleaseSamples;
    return true;
}

void Synth::start(const Note* notes, size_t count, Priority priority) {
    memcpy(_notes, notes, count * sizeof(Note));
    _count = count; _index = 0; _priority = priority; _releaseLeft = 0;
    startNote();
}

void Synth::startNote() {
    const Note& n = _notes[_index];
    _position = 0; _duration = n.durationMs * (SampleRate / 1000); _gap = n.gapMs * (SampleRate / 1000);
    float base = midiFrequency(n.midi);
    float end = midiFrequency(static_cast<uint8_t>(limit(n.midi + n.glideSemitones, 0, 127)));
    const float squareRatios[3] = {1, 3, 5}, bellRatios[3] = {1, 2.01f, 3.99f}, normalRatios[3] = {1, 2, 3};
    const float* ratios = n.timbre == Timbre::SoftSquare ? squareRatios : n.timbre == Timbre::Bell ? bellRatios : normalRatios;
    for (int i = 0; i < 3; ++i) {
        _phase[i] = 0;
        float from = base * ratios[i], to = end * ratios[i];
        if (from >= SampleRate * 0.45f || to >= SampleRate * 0.45f) { _increment[i] = 0; _glide[i] = 0; continue; }
        _increment[i] = static_cast<uint32_t>(from * (4294967296.0 / SampleRate));
        _glide[i] = static_cast<int32_t>((to - from) * (4294967296.0 / SampleRate) / _duration);
    }
    _noiseLowpass = 0;
}

void Synth::setEnabled(bool enabled) { _enabled = enabled; if (!enabled) cancel(); }
void Synth::cancel() { _pendingCount = 0; if (active() && !_releaseLeft) _releaseLeft = ReleaseSamples; }

int16_t Synth::sample() {
    const Note& n = _notes[_index];
    float envelope = 0;
    if (_position < _duration) {
        const uint32_t attack = _duration < 192 ? _duration / 3 : 96;
        const uint32_t release = _duration < 384 ? _duration / 3 : 192;
        float a = limit(static_cast<float>(_position) / attack, 0, 1);
        float r = limit(static_cast<float>(_duration - 1 - _position) / release, 0, 1);
        envelope = a * a * (3 - 2 * a) * r * r * (3 - 2 * r);
        if (n.timbre == Timbre::Bell || n.timbre == Timbre::Pluck || n.timbre == Timbre::Chime) {
            float decay = 1.0f - static_cast<float>(_position) / _duration;
            envelope *= decay * decay;
        }
    }
    float wave = 0;
    float harmonics[3] = {1, 0, 0};
    if (n.timbre == Timbre::Bell) { harmonics[0] = .70f; harmonics[1] = .21f; harmonics[2] = .09f; }
    else if (n.timbre == Timbre::SoftSquare) { harmonics[0] = .72f; harmonics[1] = .18f; harmonics[2] = .10f; }
    else if (n.timbre == Timbre::Pluck) { harmonics[0] = .70f; harmonics[1] = .20f; harmonics[2] = .10f; }
    else if (n.timbre == Timbre::Chime) { harmonics[0] = .65f; harmonics[1] = .25f; harmonics[2] = .10f; }
    for (int i = 0; i < 3; ++i) {
        if (_increment[i]) wave += sine(_phase[i]) * harmonics[i] / 32768.0f;
        _phase[i] += _increment[i]; _increment[i] += _glide[i];
    }
    if (n.timbre == Timbre::Noise) {
        _noise ^= _noise << 13; _noise ^= _noise >> 17; _noise ^= _noise << 5;
        _noiseLowpass += (static_cast<int16_t>(_noise) - _noiseLowpass) / 4;
        wave = _noiseLowpass / 32768.0f;
    }
    _gain += (_volume / 255.0f - _gain) / 64;
    if (_releaseLeft) envelope *= static_cast<float>(_releaseLeft - 1) / ReleaseSamples;
    int16_t output = static_cast<int16_t>(limit(wave * envelope * _gain * (n.velocity / 255.0f) * Peak, -Peak, Peak));
    ++_position;
    bool released = _releaseLeft && --_releaseLeft == 0;
    if (released) {
        _count = 0;
        if (_pendingCount) { uint8_t count = _pendingCount; _pendingCount = 0; start(_pending, count, _pendingPriority); }
    } else if (_position >= _duration + _gap) {
        if (++_index >= _count) {
            _count = 0;
            if (_pendingCount) { uint8_t count = _pendingCount; _pendingCount = 0; start(_pending, count, _pendingPriority); }
        } else startNote();
    }
    return output;
}

size_t Synth::render(int16_t* output, size_t capacity) {
    if (!output) return 0;
    size_t count = 0;
    while (count < capacity && active()) output[count++] = sample();
    for (size_t i = count; i < capacity; ++i) output[i] = 0;
    return count;
}
} }
