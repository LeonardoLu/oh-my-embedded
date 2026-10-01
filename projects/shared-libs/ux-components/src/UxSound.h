#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ux { namespace sound {
constexpr uint32_t SampleRate = 16000;
enum class Priority : uint8_t { Ambient, Interaction, Notification, Alert };
enum class Timbre : uint8_t { Sine, Bell, SoftSquare, Pluck, Chime, Noise };
enum class Cue : uint8_t {
    Tap, Select, Action, Confirm, Back, Open, Close, Success, Warning,
    Error, Reject, Connected, Disconnected, Wake, Sleep, Poke, Count
};
struct Note {
    uint8_t midi = 69;
    uint16_t durationMs = 90;
    uint16_t gapMs = 0;
    Timbre timbre = Timbre::Bell;
    uint8_t velocity = 180;
    int8_t glideSemitones = 0;
};
enum class Scale : uint8_t { Major, Minor, Pentatonic };
float midiFrequency(uint8_t midi);
uint8_t scaleMidi(uint8_t root, uint8_t degree, Scale scale = Scale::Pentatonic);

// render() advances sample time, returns valid mono signed-16 samples, and fills
// the unused tail with silence. Keep sources alive while attached to an output.
class Source {
public:
    virtual ~Source() = default;
    virtual size_t render(int16_t* output, size_t capacity) = 0;
    virtual bool active() const = 0;
    virtual void cancel() = 0;
};

class Synth : public Source {
public:
    static constexpr size_t MaxNotes = 8;
    bool play(Cue cue);
    bool play(Cue cue, Priority priority);
    // Notes are copied immediately. Lower-priority requests cannot interrupt.
    // Equal/higher priorities replace via an 8 ms release, never a hard cut.
    bool play(const Note* notes, size_t count, Priority priority = Priority::Interaction);
    bool play(const Note& note, Priority priority = Priority::Interaction) { return play(&note, 1, priority); }
    void setEnabled(bool enabled);
    bool enabled() const { return _enabled; }
    void setVolume(uint8_t volume) { _volume = volume; if (!active()) _gain = volume / 255.0f; }
    uint8_t volume() const { return _volume; }
    void cancel() override;
    bool active() const override { return _count != 0; }
    size_t render(int16_t* output, size_t capacity) override;
private:
    void start(const Note* notes, size_t count, Priority priority);
    void startNote();
    int16_t sample();
    Note _notes[MaxNotes]{};
    Note _pending[MaxNotes]{};
    uint8_t _count = 0, _index = 0, _pendingCount = 0;
    Priority _priority = Priority::Ambient, _pendingPriority = Priority::Ambient;
    bool _enabled = true;
    uint8_t _volume = 180;
    uint32_t _position = 0, _duration = 0, _gap = 0;
    uint32_t _phase[3]{}, _increment[3]{};
    int32_t _glide[3]{};
    uint16_t _releaseLeft = 0;
    uint32_t _noise = 0x739af012;
    int32_t _noiseLowpass = 0;
    float _gain = 180.0f / 255;
};
} }
