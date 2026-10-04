#pragma once
#include <M5Unified.h>
#include <UxSound.h>
#include <UxSoundM5.h>
#include "FeedbackLevel.h"

class AudioFeedback {
public:
    bool begin() {
        M5.Speaker.setVolume(corefeedback::kSpeakerMasterVolume);
        return _ready=_output.begin(M5.Speaker,_synth,6);
    }
    void update() { _output.update(); }
    void setEnabled(bool enabled) { _synth.setEnabled(enabled); }
    // Local synthesis gain; 0 is silent, 255 maximum. Mute preserves this value.
    void setVolume(uint8_t volume) { _synth.setVolume(volume); }
    uint8_t volume() const { return _synth.volume(); }
    uint8_t speakerVolume() const { return M5.Speaker.getVolume(); }
    void select();
    void action();
    void confirm();
    void reject();
    void error();
    uint32_t failures() const { return _output.failures(); }
    bool available() const { return _ready; }
    bool busy() const { return _output.busy(); }
private:
    bool _ready=false;
    ux::sound::Synth _synth;
    ux::sound::M5Output<m5::Speaker_Class> _output;
};
