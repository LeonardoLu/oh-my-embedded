#include "AudioFeedback.h"
void AudioFeedback::select()  { _synth.play(ux::sound::Cue::Select); }
void AudioFeedback::action()  { _synth.play(ux::sound::Cue::Action); }
void AudioFeedback::confirm() { _synth.play(ux::sound::Cue::Confirm); }
void AudioFeedback::reject()  { _synth.play(ux::sound::Cue::Reject); }
void AudioFeedback::error()   { _synth.play(ux::sound::Cue::Error); }
