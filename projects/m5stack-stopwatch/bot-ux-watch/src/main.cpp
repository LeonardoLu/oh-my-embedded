// bot-ux-watch — a round-safe M5Stack StopWatch companion face.

#include <M5Unified.h>
#include <BotUx.h>

#include "CalendarMath.h"
#include "CompanionPresets.h"
#include "InputSemantics.h"
#include "TouchContact.h"
#include "TouchAffine.h"
#include "TouchTraceBuffer.h"
#include "Power.h"
#include "RtcClock.h"
#include "PowerPolicy.h"
#include "Settings.h"
#include "TimedState.h"
#include "WatchFace.h"
#include "WatchInteraction.h"
#include "WatchButtonFeedback.h"
#include "WatchButtonFeedbackMasks.h"
#include "WatchEdgeGeometry.h"
#include "WatchFeedbackPatch.h"
#include "WatchUi.h"
#include "WatchControls.h"
#include "WatchSettingsCarousel.h"
#include "WatchSettingsDirectCanvas.h"
#include "WatchSettingsRows.h"
#include <UxPointer.h>
#include <UxSound.h>
#include <UxSoundM5.h>
#include <UxInput.h>
#include <UxKeyboard.h>
#include <Preferences.h>

#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using watchinput::Gesture;
static_assert((uint8_t)botux::BotUx::Mood::LookingAround==watchcompanion::lookingAroundMood(),
              "Looking around must remain the appended Watch ambient mood");

namespace {
constexpr int16_t kW = 466;
constexpr int16_t kH = 466;
constexpr int16_t kBotSize = 286;
constexpr int16_t kBotX = (kW - kBotSize) / 2;
constexpr int16_t kBotY = 90;
constexpr int16_t kPreviewSize = 178;
constexpr int16_t kPreviewX = (kW - kPreviewSize) / 2;
constexpr int16_t kPreviewY = 62;
constexpr int16_t kMenuStep = watchcontrols::mainList().step;
constexpr uint8_t kVisibleRows = watchcontrols::mainList().visibleRows;
constexpr uint8_t kMenuCount=(uint8_t)MenuItem::Done;
constexpr uint8_t kTimeCount=(uint8_t)TimeItem::Back;
constexpr uint8_t kPersonalCount=(uint8_t)PersonalItem::Back;
constexpr uint32_t kActiveFrameMs = 16;
constexpr uint32_t kPreviewFrameMs = 33;
constexpr uint32_t kDozeFrameMs = 250;
constexpr uint8_t kLowBattery = 15;
constexpr uint8_t kAudioPowerIo = 2;     // M5IOE1 G3, zero-based index.
constexpr uint8_t kAudioAmplifierIo = 9; // M5IOE1 G10, zero-based index.

const char* const kMenuLabels[(uint8_t)MenuItem::Count] = {
    "TIME", "BOT", "DISPLAY", "SOUND", "POWER", "DONE"
};
const char* const kTimeLabels[(uint8_t)TimeItem::Count] = {
    "TIME", "DATE", "FORMAT", "BACK"
};
const char* const kPersonalLabels[(uint8_t)PersonalItem::Count] = {
    "EXPRESSION", "ACTION", "APPEARANCE", "COLOR", "NAME", "LANGUAGE", "COMBINATIONS", "GAZE", "INTENSITY", "SPEED", "BACK"
};
const char* const kMonths[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
}

M5Canvas canvas(&M5.Display);
M5Canvas botSprite(&M5.Display);
M5Canvas previewSprite(&M5.Display);
WatchFace face;
botux::BotUx previewBot;
Settings settings;
Power power;
watchbuttons::Feedback _buttonFeedback;
lgfx::rgb565_t* _buttonFeedbackScratch = nullptr;
uint8_t* _buttonFeedbackMasks = nullptr;
uint8_t _buttonFeedbackDrawnMask = 0;
bool _buttonFeedbackDirty = false;
bool _buttonFeedbackFaceBaseValid = false;
uint32_t _buttonFeedbackRenderUs = 0;
uint32_t _buttonFeedbackRenderMaxUs = 0;
uint32_t _buttonFeedbackPixels = 0;
uint32_t _buttonFeedbackHudMs = 0;
constexpr size_t kButtonScratchPixels = 160u * 187u;
constexpr size_t kButtonSideMaskPixels = 160u * 187u;
constexpr size_t kButtonPowerMaskPixels = 155u * 155u;
constexpr size_t kButtonMaskBytes = watchbuttons::Feedback::ExpandSteps + 1;
constexpr size_t kButtonMasksTotal = kButtonMaskBytes
    * (kButtonSideMaskPixels * 2 + kButtonPowerMaskPixels);

Screen _screen = Screen::Face;
Editor _editor = Editor::None;
MenuItem _menu = MenuItem::Time;
TimeItem _timeItem = TimeItem::Time;
PersonalItem _personal = PersonalItem::Expression;
Settings::Data _originalSettings;
Screen _editorParent = Screen::Settings;
Screen _personalParent = Screen::Settings;

watchinput::ButtonGesture _buttonA;
watchinput::ButtonGesture _buttonB;
watchinput::TouchGesture _touch;
watchinput::TouchContact _contact;
watchinput::RegionDoubleTap _clockDoubleTap;
uint32_t _contactReadMs=0, _gazeTouchMs=0;
bool _faceTracking=false;
bool _diagnosticContact=false, _diagnosticDown=false, _contactReplyPending=false;
int16_t _diagnosticX=0, _diagnosticY=0;
watchinteraction::SingleDoubleClick _bClick;
ux::PointerSession _pointer;
ux::Rect _clickRect{0,0,0,0};
uint32_t _clickUntil=0;
int _capturedTarget=watchcontrols::None;
bool _soundBound=false;
ux::sound::Synth _sounds;
ux::sound::M5Output<m5::Speaker_Class> _soundOutput;
bool _diagnosticScroll = false;
int _namePressedKey = -1;
watchinteraction::SettingsChord _settingsChord;
watchcontrols::CarouselModel _settingsCarousel;
watchinteraction::ScrollList _timeList;
watchinteraction::ScrollList _personalList;
ux::ScrollModel _timeScroll, _personalScroll;
ux::ScrollModel _editorScroll;
ux::NameEditor _nameEditor;
uint32_t _scrollMs = 0;
uint8_t _previewMood = 0, _previewExpression = 0, _previewAnimation = 0;
bool _manualPreset = false;
botux::BotUx::Preset _manualCombo{botux::BotUx::Mood::Idle, botux::BotUx::Expression::Auto, botux::BotUx::Animation::Auto};
watchcompanion::MoodDeck _moodDeck(botux::BotUx::moodCount());
uint32_t _gazeUntil = 0;
bool _originalManualPreset = false;
botux::BotUx::Preset _originalManualCombo = _manualCombo;
watchinteraction::MotionFilter _motionFilter;
watchinteraction::AmbientCycle _ambientCycle;
int16_t _tapX = 0, _tapY = 0;

uint8_t _battery = 100;
bool _charging = false;
bool _powerKnown = false;
uint32_t _powerReadMs = 0;
uint32_t _powerButtonReadMs = 0;
bool _powerButtonValid = false, _powerButtonPressed = false;
bool _powerButtonWakePressed = false;
watchpower::WakeInputGate _wakeInputGate;
bool _externalPower = false;
bool _diagnosticButtons = false;
uint8_t _diagnosticButtonMask = 0;
bool _diagnosticBattery = false;
uint8_t _diagnosticBatteryPct = 100;
TimedState _happyAcknowledgment;
TimedState _pokeReaction;
uint32_t _statusPanelStartMs = 0;
uint32_t _statusPanelUntilMs = 0;
int16_t _statusPanelTouchOffset = 0;
bool _statusPanelTouchVisible = false;
bool _dozing = false;
bool _manualDozing = false;
watchpower::ScreenState _screenPowerState = watchpower::ScreenState::Active;
watchpower::IdleScreenPolicy _idleScreenPolicy;
bool _renderReady = false;
bool _uiDirty = true;
bool _rtcSaveFailed = false;
bool _listBandOnly = false;
bool _faceNeedsClear = true;
uint32_t _lastFrameMs = 0;
uint32_t _lastImuReadMs = 0;
botux::BotUx::Mood _ambientMood = botux::BotUx::Mood::Idle;
botux::BotUx::Mood _drawMood = (botux::BotUx::Mood)0xFF;
uint8_t _colorDrag = 0;
bool _gestureActive = false;
bool _buttonNavigation = false;

enum class TouchTraceKind : uint8_t { Sample, Down, Scroll, End, Screen };
enum class TouchEndReason : uint8_t { Accepted, NoTarget, Scrolled, TargetChanged, Timeout, Outside };
struct TouchTraceEvent {
    uint32_t atMs;
    uint32_t acquisition, irq;
    int16_t sensorX, sensorY, x, y;
    int16_t target, other;
    uint16_t elapsedMs, sampleGapMs, readUs;
    uint8_t kind, screen, flags;
};
constexpr uint16_t kTouchTraceDetailCapacity = 128;
constexpr uint16_t kTouchTraceCriticalCapacity = 128;
watchtrace::Ring<TouchTraceEvent,kTouchTraceDetailCapacity> _touchTraceDetail;
watchtrace::Ring<TouchTraceEvent,kTouchTraceCriticalCapacity> _touchTraceCritical;
uint32_t _traceLastHeldMs = 0, _traceAcquireCount = 0;
uint16_t _traceMaxSampleGapMs = 0, _traceMaxReadUs = 0;
volatile bool _touchTraceEnabled = false;
bool _traceRawKnown = false, _traceRawContact = false;
bool _traceSampleSensorKnown = false, _traceLastSensorKnown = false;
int16_t _traceSampleSensorX = 0, _traceSampleSensorY = 0;
int16_t _traceLastSensorX = 0, _traceLastSensorY = 0;
uint32_t _traceLastSensorAcquisition = 0;
bool _touchIrqAttached = false;
constexpr uint8_t kTouchIrqCapacity = 64;
volatile uint32_t _touchIrqTimes[kTouchIrqCapacity];
volatile uint32_t _touchIrqTotal = 0, _touchIrqDropped = 0;
volatile uint8_t _touchIrqHead = 0, _touchIrqCount = 0;

struct TouchCalibrationResult {
    int16_t targetX, targetY;
    int16_t firstX, firstY, stableX, stableY;
    int16_t firstRawX, firstRawY, stableRawX, stableRawY;
    int16_t minRawX, minRawY, maxRawX, maxRawY;
    uint16_t samples, heldMs;
    float predictedX, predictedY, error;
};
constexpr uint8_t kTouchCalibrationCount = 5;
constexpr uint8_t kTouchRepeatCount = 9;
constexpr uint8_t kTouchDirectionCount = 6;
const int16_t kTouchCalibrationTargets[kTouchCalibrationCount][2] = {
    {233,233}, {233,100}, {100,233}, {366,233}, {233,399}
};
const int16_t kTouchValidationTargets[kTouchCalibrationCount][2] = {
    {145,145}, {321,145}, {145,321}, {321,321}, {233,410}
};
const int16_t kTouchRepeatTargets[kTouchRepeatCount][2] = {
    {233,233}, {321,321}, {233,399},
    {321,321}, {233,399}, {233,233},
    {233,399}, {233,233}, {321,321}
};
const int16_t kTouchDirectionTargets[kTouchDirectionCount][2] = {
    {233,233}, {233,233}, {233,233}, {233,233}, {233,233}, {233,233}
};
const bool kTouchDirectionTurned[kTouchDirectionCount] = {
    false, true, true, false, false, true
};
enum class TouchCalibrationMode : uint8_t {
    Probe, Train, Verify, Passed, Failed, Repeat, Direction
};
TouchCalibrationResult _touchCalibrationResults[kTouchCalibrationCount];
TouchCalibrationResult _touchValidationResults[kTouchCalibrationCount];
TouchCalibrationResult _touchRepeatResults[kTouchRepeatCount];
TouchCalibrationMode _touchCalibrationMode=TouchCalibrationMode::Probe;
bool _touchCalibration = false, _touchCalibrationTracking = false, _touchCalibrationAwaitRelease = false;
bool _touchCalibrationRetry=false;
bool _touchCalibrationDirectionGate=false;
uint8_t _touchCalibrationIndex = 0, _touchCalibrationReleaseSamples = 0;
int16_t _touchRawX = 0, _touchRawY = 0;
int32_t _touchCalibrationSumX = 0, _touchCalibrationSumY = 0;
int32_t _touchCalibrationRawSumX = 0, _touchCalibrationRawSumY = 0;
uint32_t _touchCalibrationStartedMs = 0;
uint32_t _touchCalibrationLastContactMs = 0;
Screen _touchCalibrationPriorScreen = Screen::Face;
watchinput::TouchAffineResult _touchCalibrationCandidate{};
watchinput::TouchAffineError _touchValidationError{};
constexpr float kIdentityTouchAffine[6]={1,0,0,0,1,0};
float _touchAffineActive[6]={1,0,0,0,1,0};
float _touchAffineBaseline[6]={1,0,0,0,1,0};
bool _touchAffineStored=false, _touchCalibrationCandidateVerified=false;
constexpr uint32_t kTouchAffineMagic=0x57414631;
constexpr uint16_t kTouchAffineVersion=1;
constexpr int16_t kTouchCalibrationMaxSpan=16;
constexpr float kTouchCalibrationMaxError=10.0f;
constexpr float kTouchCalibrationMaxRms=7.0f;
struct TouchAffineBlob {
    uint32_t magic;
    uint16_t version;
    uint16_t bytes;
    float coefficients[6];
};

uint8_t _editHour = 0;
uint8_t _editMinute = 0;
int16_t _editYear = 2026;
uint8_t _editMonth = 1;
uint8_t _editDay = 1;
uint8_t _editField = 0;

struct FrameTelemetry {
    uint32_t windowStartMs = 0;
    uint32_t frames = 0;
    uint64_t updateUs = 0;
    uint64_t drawUs = 0;
    uint64_t pushUs = 0;
    uint32_t maxFrameUs = 0;
    uint8_t screen = 0xFF;
} _telemetry;

static uint16_t traceClamp16(uint32_t value) { return value > 65535 ? 65535 : (uint16_t)value; }

static void IRAM_ATTR touchTraceIrq() {
    if (!_touchTraceEnabled) return;
    uint8_t index = (_touchIrqHead + _touchIrqCount) % kTouchIrqCapacity;
    if (_touchIrqCount == kTouchIrqCapacity) {
        index = _touchIrqHead;
        _touchIrqHead = (_touchIrqHead + 1) % kTouchIrqCapacity;
        ++_touchIrqDropped;
    } else {
        ++_touchIrqCount;
    }
    _touchIrqTimes[index] = micros();
    ++_touchIrqTotal;
}

static void pushTouchTrace(TouchTraceKind kind, uint32_t atMs, int16_t x, int16_t y,
                           int16_t target = watchcontrols::None, int16_t other = watchcontrols::None,
                           uint32_t elapsedMs = 0, uint32_t sampleGapMs = 0,
                           uint32_t readUs = 0, uint8_t flags = 0) {
    if (!_touchTraceEnabled) return;
    bool end=kind==TouchTraceKind::End;
    bool sensorKnown=end?_traceLastSensorKnown:_traceSampleSensorKnown;
    if(sensorKnown) flags|=0x80;
    TouchTraceEvent event{atMs,end?_traceLastSensorAcquisition:_traceAcquireCount,_touchIrqTotal,
                          end?_traceLastSensorX:_traceSampleSensorX,
                          end?_traceLastSensorY:_traceSampleSensorY,x,y,target,other,
                          traceClamp16(elapsedMs),traceClamp16(sampleGapMs),traceClamp16(readUs),
                          (uint8_t)kind,(uint8_t)_screen,flags};
    if(kind==TouchTraceKind::Down||kind==TouchTraceKind::End||kind==TouchTraceKind::Screen) {
        _touchTraceCritical.push(event);
    } else {
        _touchTraceDetail.push(event);
    }
}

static void setTouchTraceEnabled(bool enabled) {
    if(_touchIrqAttached) {
        detachInterrupt(digitalPinToInterrupt(13));
        _touchIrqAttached=false;
    }
    _touchTraceEnabled = enabled;
    _touchTraceDetail.clear();
    _touchTraceCritical.clear();
    _traceLastHeldMs = 0;
    _traceAcquireCount = 0;
    _traceMaxSampleGapMs = _traceMaxReadUs = 0;
    _traceRawKnown = false;
    _traceRawContact = false;
    _traceSampleSensorKnown = _traceLastSensorKnown = false;
    _traceSampleSensorX = _traceSampleSensorY = 0;
    _traceLastSensorX = _traceLastSensorY = 0;
    _traceLastSensorAcquisition = 0;
    _touchIrqHead = _touchIrqCount = 0;
    _touchIrqTotal = _touchIrqDropped = 0;
    if(enabled) {
        attachInterrupt(digitalPinToInterrupt(13), touchTraceIrq, FALLING);
        _touchIrqAttached=true;
    }
}

static void stopTouchTrace() {
    _touchTraceEnabled = false;
    if(_touchIrqAttached) {
        detachInterrupt(digitalPinToInterrupt(13));
        _touchIrqAttached=false;
    }
}

static const char* touchTraceKindName(TouchTraceKind kind) {
    switch (kind) {
        case TouchTraceKind::Sample: return "sample";
        case TouchTraceKind::Down: return "down";
        case TouchTraceKind::Scroll: return "scroll";
        case TouchTraceKind::End: return "end";
        case TouchTraceKind::Screen: return "screen";
    }
    return "?";
}

static const char* touchEndReasonName(TouchEndReason reason) {
    switch (reason) {
        case TouchEndReason::Accepted: return "accepted";
        case TouchEndReason::NoTarget: return "no_target";
        case TouchEndReason::Scrolled: return "scrolled";
        case TouchEndReason::TargetChanged: return "target_changed";
        case TouchEndReason::Timeout: return "timeout";
        case TouchEndReason::Outside: return "outside";
    }
    return "?";
}

static void dumpTouchTrace() {
    bool wasEnabled = _touchTraceEnabled;
    stopTouchTrace();
    Serial.printf("TRACE begin detail=%u detail_dropped=%lu critical=%u critical_dropped=%lu enabled=%u acquisitions=%lu max_gap=%u max_read_us=%u irq=%lu irq_kept=%u irq_dropped=%lu\n",
                  (unsigned)_touchTraceDetail.count(),(unsigned long)_touchTraceDetail.dropped(),
                  (unsigned)_touchTraceCritical.count(),(unsigned long)_touchTraceCritical.dropped(),wasEnabled,
                  (unsigned long)_traceAcquireCount, _traceMaxSampleGapMs, _traceMaxReadUs,
                  (unsigned long)_touchIrqTotal, _touchIrqCount, (unsigned long)_touchIrqDropped);
    for (uint8_t i = 0; i < _touchIrqCount; ++i) {
        Serial.printf("TRACEIRQ t_us=%lu\n",
                      (unsigned long)_touchIrqTimes[(_touchIrqHead + i) % kTouchIrqCapacity]);
    }
    size_t detailIndex=0,criticalIndex=0;
    while(detailIndex<_touchTraceDetail.count()||criticalIndex<_touchTraceCritical.count()) {
        bool useDetail=criticalIndex==_touchTraceCritical.count()
            ||(detailIndex<_touchTraceDetail.count()
               &&_touchTraceDetail.at(detailIndex).atMs<=_touchTraceCritical.at(criticalIndex).atMs);
        const auto& e=useDetail?_touchTraceDetail.at(detailIndex++):_touchTraceCritical.at(criticalIndex++);
        auto kind = (TouchTraceKind)e.kind;
        bool sensorKnown=(e.flags&0x80)!=0;
        if (kind == TouchTraceKind::Sample) {
            Serial.printf("TRACE t=%lu kind=%s screen=%u contact=%u int_low=%u sensor=%s%d,%d logical=%d,%d gap=%u read_us=%u acquisition=%lu irq=%lu\n",
                          (unsigned long)e.atMs, touchTraceKindName(kind), e.screen,
                          (e.flags & 1) != 0, (e.flags & 2) != 0,sensorKnown?"":"na:",
                          e.sensorX,e.sensorY,e.x,e.y,e.sampleGapMs,e.readUs,
                          (unsigned long)e.acquisition,(unsigned long)e.irq);
        } else if (kind == TouchTraceKind::End) {
            Serial.printf("TRACE t=%lu kind=%s screen=%u sensor_last=%s%d,%d logical=%d,%d captured=%d released=%d elapsed=%u reason=%s last_sensor_acquisition=%lu irq=%lu\n",
                          (unsigned long)e.atMs,touchTraceKindName(kind),e.screen,sensorKnown?"":"na:",
                          e.sensorX,e.sensorY,e.x,e.y,e.target,e.other,e.elapsedMs,
                          touchEndReasonName((TouchEndReason)(e.flags&0x7F)),
                          (unsigned long)e.acquisition,(unsigned long)e.irq);
        } else if (kind == TouchTraceKind::Screen) {
            Serial.printf("TRACE t=%lu kind=%s from=%d to=%d cause=%d acquisition=%lu irq=%lu\n",
                          (unsigned long)e.atMs,touchTraceKindName(kind),e.target,e.other,e.flags&0x7F,
                          (unsigned long)e.acquisition,(unsigned long)e.irq);
        } else {
            Serial.printf("TRACE t=%lu kind=%s screen=%u sensor=%s%d,%d logical=%d,%d target=%d elapsed=%u acquisition=%lu irq=%lu\n",
                          (unsigned long)e.atMs,touchTraceKindName(kind),e.screen,sensorKnown?"":"na:",
                          e.sensorX,e.sensorY,e.x,e.y,e.target,e.elapsedMs,
                          (unsigned long)e.acquisition,(unsigned long)e.irq);
        }
    }
    Serial.println("TRACE end");
    Serial.flush();
}

static bool hit(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x < left + width && y >= top && y < top + height;
}

static bool tapHit(int16_t left, int16_t top, int16_t width, int16_t height) {
    return hit(_tapX, _tapY, left, top, width, height)
        && hit(_touch.startX(), _touch.startY(), left, top, width, height);
}

static void directFaceGaze(int16_t x,int16_t y,uint32_t now) {
    const auto& metrics=face.bot().metrics();
    auto gaze=watchinteraction::touchGaze(x,y,kBotX+metrics.cx,kBotY+metrics.cy,metrics.bodyR);
    _gazeUntil=now+2200;
    face.bot().setMood(botux::BotUx::Mood::Idle);
    face.bot().setExpression(botux::BotUx::Expression::Auto,180);
    face.bot().setAnimation(botux::BotUx::Animation::Auto);
    face.bot().setTalking(false);
    _drawMood=botux::BotUx::Mood::Idle;
    face.bot().gazeAt(gaze.x,gaze.y,2200);
}

static void markListMoved() {
    if (!_uiDirty) _listBandOnly = true;
    _uiDirty = true;
}

static void revealRow(ux::ScrollModel& scroll, uint8_t index) {
    const auto layout=watchcontrols::mainList();
    float top = index * kMenuStep;
    if (top < scroll.offset()) scroll.setOffset(top);
    else if (top + kMenuStep > scroll.offset() + layout.h)
        scroll.setOffset(top + kMenuStep - layout.h);
}

static bool soundDesired() {
    return watchpower::audioShouldRun(_screenPowerState,settings.data().sound);
}

enum class SoundChannel : uint8_t { Startup, Button, Alert };

static bool soundChannelEnabled(SoundChannel channel) {
    if (!soundDesired()) return false;
    switch (channel) {
        case SoundChannel::Startup: return settings.data().startupSound;
        case SoundChannel::Button: return settings.data().buttonSound;
        default: return settings.data().alertSound;
    }
}

static bool playSound(ux::sound::Cue cue, SoundChannel channel) {
    return soundChannelEnabled(channel) && _soundOutput.ready()
        && _sounds.play(cue);
}

static SoundChannel cueChannel(ux::sound::Cue cue) {
    switch (cue) {
        case ux::sound::Cue::Tap:
        case ux::sound::Cue::Select:
        case ux::sound::Cue::Action:
        case ux::sound::Cue::Confirm:
        case ux::sound::Cue::Back:
        case ux::sound::Cue::Open:
        case ux::sound::Cue::Close:
            return SoundChannel::Button;
        case ux::sound::Cue::Wake:
            return SoundChannel::Startup;
        default:
            return SoundChannel::Alert;
    }
}

static void applySoundDemand() {
    bool desired=soundDesired();
    if(desired) {
        if(_soundBound) _soundOutput.resume();
        _sounds.setEnabled(true);
    } else {
        _sounds.setEnabled(false);
        if(_soundBound) _soundOutput.suspend();
    }
}

static void clickSound()   { playSound(ux::sound::Cue::Select, SoundChannel::Button); }
static void confirmSound() { playSound(ux::sound::Cue::Confirm, SoundChannel::Button); }
static void pokeSound()    { playSound(ux::sound::Cue::Poke, SoundChannel::Alert); }

static watchpower::IdleConfig idlePowerConfig() {
    const auto& data = settings.data();
    watchpower::IdleConfig config;
    config.enabled = data.powerSaveEnabled;
    config.dimTimeout = data.dimTimeout;
    config.offTimeout = data.screenOffTimeout;
    config.keepAwakeWhileExternalPower = data.keepAwakeWhileCharging;
    config.forcedSleepEnabled = data.forcedSleepEnabled;
    config.forcedSleepStartHour = data.forcedSleepStartHour;
    config.forcedSleepEndHour = data.forcedSleepEndHour;
    return config;
}

static bool powerClockReady() {
    return watchClock.hasTime() && watchClock.state() == RtcClock::State::Ready;
}

static uint8_t powerClockHour() {
    return powerClockReady() ? (uint8_t)watchClock.value().time.hours : 0;
}

static void resetUiPointer() {
    _faceTracking=false; _pointer.cancel(); _clickUntil=0; _settingsCarousel.cancel();
    _timeScroll.cancel(); _personalScroll.cancel(); _editorScroll.cancel();
    _capturedTarget=watchcontrols::None; _namePressedKey = -1; _colorDrag = 0;
}

static bool validTouchAffine(const float coefficients[6]) {
    if(!coefficients) return false;
    for(uint8_t i=0;i<6;++i) if(!isfinite(coefficients[i])) return false;
    float determinant=coefficients[0]*coefficients[4]-coefficients[1]*coefficients[3];
    return fabsf(determinant)>=0.01f
        && fabsf(coefficients[0])<=4&&fabsf(coefficients[1])<=4
        && fabsf(coefficients[3])<=4&&fabsf(coefficients[4])<=4
        && fabsf(coefficients[2])<=1000&&fabsf(coefficients[5])<=1000;
}

static void applyTouchAffine(const float coefficients[6]) {
    float copy[6];
    memcpy(copy,coefficients,sizeof(copy));
    M5.Display.panel()->setCalibrateAffine(copy);
}

static bool loadTouchAffine() {
    memcpy(_touchAffineActive,kIdentityTouchAffine,sizeof(_touchAffineActive));
    _touchAffineStored=false;
    Preferences preferences;
    if(preferences.begin("watch-touch",true)) {
        TouchAffineBlob blob{};
        bool complete=preferences.isKey("affine")
            &&preferences.getBytesLength("affine")==sizeof(blob)
            &&preferences.getBytes("affine",&blob,sizeof(blob))==sizeof(blob);
        preferences.end();
        if(complete&&blob.magic==kTouchAffineMagic&&blob.version==kTouchAffineVersion
           &&blob.bytes==sizeof(blob)&&validTouchAffine(blob.coefficients)) {
            memcpy(_touchAffineActive,blob.coefficients,sizeof(_touchAffineActive));
            _touchAffineStored=true;
        }
    }
    applyTouchAffine(_touchAffineActive);
    return _touchAffineStored;
}

static bool saveTouchAffine() {
    if(!_touchCalibrationCandidateVerified||!validTouchAffine(_touchCalibrationCandidate.coefficients)) return false;
    TouchAffineBlob blob{kTouchAffineMagic,kTouchAffineVersion,sizeof(TouchAffineBlob),{}};
    memcpy(blob.coefficients,_touchCalibrationCandidate.coefficients,sizeof(blob.coefficients));
    Preferences preferences;
    bool ok=preferences.begin("watch-touch",false);
    if(ok) {
        ok=preferences.putBytes("affine",&blob,sizeof(blob))==sizeof(blob);
        preferences.end();
    }
    if(ok) {
        memcpy(_touchAffineActive,blob.coefficients,sizeof(_touchAffineActive));
        _touchAffineStored=true;
    }
    return ok;
}

static bool resetTouchAffine() {
    Preferences preferences;
    bool ok=preferences.begin("watch-touch",false);
    if(ok) { ok=preferences.clear(); preferences.end(); }
    memcpy(_touchAffineActive,kIdentityTouchAffine,sizeof(_touchAffineActive));
    _touchAffineStored=false;
    _touchCalibrationCandidateVerified=false;
    _touchCalibrationMode=TouchCalibrationMode::Probe;
    applyTouchAffine(_touchAffineActive);
    return ok;
}

static const int16_t (*touchCalibrationTargets())[2] {
    if(_touchCalibrationMode==TouchCalibrationMode::Verify) return kTouchValidationTargets;
    if(_touchCalibrationMode==TouchCalibrationMode::Repeat) return kTouchRepeatTargets;
    if(_touchCalibrationMode==TouchCalibrationMode::Direction) return kTouchDirectionTargets;
    return kTouchCalibrationTargets;
}

static TouchCalibrationResult* touchCalibrationResults() {
    if(_touchCalibrationMode==TouchCalibrationMode::Verify) return _touchValidationResults;
    if(_touchCalibrationMode==TouchCalibrationMode::Repeat
       ||_touchCalibrationMode==TouchCalibrationMode::Direction) return _touchRepeatResults;
    return _touchCalibrationResults;
}

static uint8_t touchCalibrationCount() {
    if(_touchCalibrationMode==TouchCalibrationMode::Repeat) return kTouchRepeatCount;
    if(_touchCalibrationMode==TouchCalibrationMode::Direction) return kTouchDirectionCount;
    return kTouchCalibrationCount;
}

static void startTouchCalibration(TouchCalibrationMode mode=TouchCalibrationMode::Probe) {
    resetUiPointer();
    _diagnosticContact=false;
    _touchCalibrationPriorScreen=_screen;
    memcpy(_touchAffineBaseline,_touchAffineActive,sizeof(_touchAffineBaseline));
    applyTouchAffine(_touchAffineBaseline);
    _touchCalibration=true;
    _touchCalibrationMode=mode;
    _touchCalibrationTracking=false;
    _touchCalibrationAwaitRelease=true;
    _touchCalibrationRetry=false;
    _touchCalibrationDirectionGate=mode==TouchCalibrationMode::Direction;
    _touchCalibrationReleaseSamples=0;
    _touchCalibrationIndex=0;
    memset(_touchCalibrationResults,0,sizeof(_touchCalibrationResults));
    memset(_touchValidationResults,0,sizeof(_touchValidationResults));
    memset(_touchRepeatResults,0,sizeof(_touchRepeatResults));
    _touchCalibrationCandidate={};
    _touchValidationError={};
    _touchCalibrationCandidateVerified=false;
    _touchCalibrationLastContactMs=millis();
    _contact.sample(false,0,0);
    setTouchTraceEnabled(true);
    _uiDirty=true;
    _listBandOnly=false;
}

static void stopTouchCalibration() {
    if(!_touchCalibration) return;
    if(_touchCalibrationMode!=TouchCalibrationMode::Passed) {
        applyTouchAffine(_touchAffineBaseline);
    }
    _touchCalibration=false;
    _touchCalibrationTracking=false;
    _touchCalibrationAwaitRelease=false;
    _touchCalibrationRetry=false;
    _touchCalibrationDirectionGate=false;
    _touchCalibrationReleaseSamples=0;
    stopTouchTrace();
    _contact.sample(false,0,0);
    resetUiPointer();
    _screen=_touchCalibrationPriorScreen;
    _uiDirty=true;
    _listBandOnly=false;
    if(_screen==Screen::Face) { _faceNeedsClear=true; face.invalidate(); }
}

static void sampleTouchCalibration(bool contact, bool acquired, int16_t x, int16_t y,
                                   uint32_t now) {
    if(_touchCalibrationMode==TouchCalibrationMode::Passed
       ||_touchCalibrationMode==TouchCalibrationMode::Failed) return;
    if(!acquired||_touchCalibrationIndex>=touchCalibrationCount()) return;
    if(_touchCalibrationAwaitRelease) {
        if(contact) {
            _touchCalibrationReleaseSamples=0;
            _touchCalibrationLastContactMs=now;
        } else if(++_touchCalibrationReleaseSamples>=3
                  && now-_touchCalibrationLastContactMs>=24) {
            _touchCalibrationAwaitRelease=false;
            _touchCalibrationReleaseSamples=0;
            _uiDirty=true;
            _listBandOnly=false;
        }
        return;
    }
    auto& result=touchCalibrationResults()[_touchCalibrationIndex];
    if(!_touchCalibrationTracking&&contact) {
        const auto* targets=touchCalibrationTargets();
        result={targets[_touchCalibrationIndex][0],targets[_touchCalibrationIndex][1],
                x,y,x,y,
                _touchRawX,_touchRawY,_touchRawX,_touchRawY,
                _touchRawX,_touchRawY,_touchRawX,_touchRawY,1,0,0,0,0};
        _touchCalibrationSumX=x;
        _touchCalibrationSumY=y;
        _touchCalibrationRawSumX=_touchRawX;
        _touchCalibrationRawSumY=_touchRawY;
        _touchCalibrationStartedMs=now;
        _touchCalibrationLastContactMs=now;
        _touchCalibrationReleaseSamples=0;
        _touchCalibrationTracking=true;
        _touchCalibrationRetry=false;
        return;
    }
    if(contact&&_touchCalibrationTracking) {
        _touchCalibrationSumX+=x;
        _touchCalibrationSumY+=y;
        _touchCalibrationRawSumX+=_touchRawX;
        _touchCalibrationRawSumY+=_touchRawY;
        if(_touchRawX<result.minRawX) result.minRawX=_touchRawX;
        if(_touchRawY<result.minRawY) result.minRawY=_touchRawY;
        if(_touchRawX>result.maxRawX) result.maxRawX=_touchRawX;
        if(_touchRawY>result.maxRawY) result.maxRawY=_touchRawY;
        if(result.samples<65535) ++result.samples;
        _touchCalibrationLastContactMs=now;
        _touchCalibrationReleaseSamples=0;
        return;
    }
    if(!contact&&_touchCalibrationTracking
       && ++_touchCalibrationReleaseSamples>=3
       && now-_touchCalibrationLastContactMs>=24) {
        uint16_t count=result.samples?result.samples:1;
        result.stableX=(int16_t)(_touchCalibrationSumX/count);
        result.stableY=(int16_t)(_touchCalibrationSumY/count);
        result.stableRawX=(int16_t)(_touchCalibrationRawSumX/count);
        result.stableRawY=(int16_t)(_touchCalibrationRawSumY/count);
        result.heldMs=traceClamp16(now-_touchCalibrationStartedMs);
        _touchCalibrationTracking=false;
        _touchCalibrationReleaseSamples=0;
        int16_t spanX=result.maxRawX-result.minRawX;
        int16_t spanY=result.maxRawY-result.minRawY;
        if(result.samples<2||spanX>kTouchCalibrationMaxSpan||spanY>kTouchCalibrationMaxSpan) {
            memset(&result,0,sizeof(result));
            _touchCalibrationRetry=true;
            _uiDirty=true;
            _listBandOnly=false;
            return;
        }
        ++_touchCalibrationIndex;
        if(_touchCalibrationMode==TouchCalibrationMode::Direction
           &&_touchCalibrationIndex<kTouchDirectionCount
           &&kTouchDirectionTurned[_touchCalibrationIndex]
              !=kTouchDirectionTurned[_touchCalibrationIndex-1]) {
            _touchCalibrationDirectionGate=true;
        }
        if(_touchCalibrationIndex==kTouchCalibrationCount
           &&_touchCalibrationMode==TouchCalibrationMode::Train) {
            watchinput::TouchAffinePoint points[kTouchCalibrationCount];
            for(uint8_t i=0;i<kTouchCalibrationCount;++i) {
                const auto& r=_touchCalibrationResults[i];
                points[i]={(float)r.stableRawX,(float)r.stableRawY,
                           (float)r.targetX,(float)r.targetY};
            }
            bool solved=watchinput::solveTouchAffine(points,kTouchCalibrationCount,
                                                     &_touchCalibrationCandidate);
            if(!solved||!validTouchAffine(_touchCalibrationCandidate.coefficients)
               ||_touchCalibrationCandidate.trainingError.maximum>kTouchCalibrationMaxError) {
                _touchCalibrationMode=TouchCalibrationMode::Failed;
                applyTouchAffine(_touchAffineBaseline);
            } else {
                applyTouchAffine(_touchCalibrationCandidate.coefficients);
                _touchCalibrationMode=TouchCalibrationMode::Verify;
                _touchCalibrationIndex=0;
                _touchCalibrationAwaitRelease=true;
                _touchCalibrationReleaseSamples=0;
                _touchCalibrationLastContactMs=now;
            }
        } else if(_touchCalibrationIndex==kTouchCalibrationCount
                  &&_touchCalibrationMode==TouchCalibrationMode::Verify) {
            double squared=0;
            float maximum=0;
            for(uint8_t i=0;i<kTouchCalibrationCount;++i) {
                auto& r=_touchValidationResults[i];
                r.predictedX=_touchCalibrationCandidate.coefficients[0]*r.stableRawX
                    +_touchCalibrationCandidate.coefficients[1]*r.stableRawY
                    +_touchCalibrationCandidate.coefficients[2];
                r.predictedY=_touchCalibrationCandidate.coefficients[3]*r.stableRawX
                    +_touchCalibrationCandidate.coefficients[4]*r.stableRawY
                    +_touchCalibrationCandidate.coefficients[5];
                float dx=r.stableX-r.targetX,dy=r.stableY-r.targetY;
                r.error=sqrtf(dx*dx+dy*dy);
                squared+=r.error*r.error;
                if(r.error>maximum) maximum=r.error;
            }
            _touchValidationError={(float)sqrt(squared/kTouchCalibrationCount),maximum};
            _touchCalibrationCandidateVerified=maximum<=kTouchCalibrationMaxError
                &&_touchValidationError.rms<=kTouchCalibrationMaxRms;
            _touchCalibrationMode=_touchCalibrationCandidateVerified
                ?TouchCalibrationMode::Passed:TouchCalibrationMode::Failed;
            if(!_touchCalibrationCandidateVerified) applyTouchAffine(_touchAffineBaseline);
        }
        _uiDirty=true;
        _listBandOnly=false;
    }
}

static void dumpTouchCalibration() {
    dumpTouchTrace();
    Serial.printf("CAL begin mode=%u index=%u verified=%u stored=%u\n",
                  (unsigned)_touchCalibrationMode,_touchCalibrationIndex,
                  _touchCalibrationCandidateVerified,_touchAffineStored);
    for(uint8_t i=0;i<kTouchCalibrationCount&&_touchCalibrationResults[i].samples;++i) {
        const auto& r=_touchCalibrationResults[i];
        Serial.printf("CAL train=%u target=%d,%d logical=%d,%d sensor=%d,%d range=%d,%d..%d,%d samples=%u held=%u\n",
                      i+1,r.targetX,r.targetY,r.stableX,r.stableY,r.stableRawX,r.stableRawY,
                      r.minRawX,r.minRawY,r.maxRawX,r.maxRawY,r.samples,r.heldMs);
    }
    if(validTouchAffine(_touchCalibrationCandidate.coefficients)) {
        Serial.printf("CAL candidate=%.8f,%.8f,%.8f,%.8f,%.8f,%.8f train_rms=%.3f train_max=%.3f\n",
                      _touchCalibrationCandidate.coefficients[0],_touchCalibrationCandidate.coefficients[1],
                      _touchCalibrationCandidate.coefficients[2],_touchCalibrationCandidate.coefficients[3],
                      _touchCalibrationCandidate.coefficients[4],_touchCalibrationCandidate.coefficients[5],
                      _touchCalibrationCandidate.trainingError.rms,
                      _touchCalibrationCandidate.trainingError.maximum);
    }
    for(uint8_t i=0;i<kTouchCalibrationCount&&_touchValidationResults[i].samples;++i) {
        const auto& r=_touchValidationResults[i];
        Serial.printf("CAL verify=%u target=%d,%d sensor=%d,%d converted=%d,%d predicted=%.2f,%.2f error=%.3f range=%d,%d..%d,%d samples=%u held=%u\n",
                      i+1,r.targetX,r.targetY,r.stableRawX,r.stableRawY,r.stableX,r.stableY,
                      r.predictedX,r.predictedY,r.error,r.minRawX,r.minRawY,r.maxRawX,r.maxRawY,
                      r.samples,r.heldMs);
    }
    if(_touchValidationResults[0].samples) {
        Serial.printf("CAL verify_rms=%.3f verify_max=%.3f thresholds=%.1f,%.1f\n",
                      _touchValidationError.rms,_touchValidationError.maximum,
                      kTouchCalibrationMaxRms,kTouchCalibrationMaxError);
    }
    uint8_t resultCount=_touchCalibrationMode==TouchCalibrationMode::Direction
        ?kTouchDirectionCount:kTouchRepeatCount;
    for(uint8_t i=0;i<resultCount&&_touchRepeatResults[i].samples;++i) {
        const auto& r=_touchRepeatResults[i];
        if(_touchCalibrationMode==TouchCalibrationMode::Direction) {
            Serial.printf("CAL direction=%u phase=%s target=%d,%d sensor=%d,%d converted=%d,%d range=%d,%d..%d,%d samples=%u held=%u\n",
                          i+1,kTouchDirectionTurned[i]?"turned":"normal",r.targetX,r.targetY,
                          r.stableRawX,r.stableRawY,r.stableX,r.stableY,
                          r.minRawX,r.minRawY,r.maxRawX,r.maxRawY,r.samples,r.heldMs);
        } else {
            Serial.printf("CAL repeat=%u target=%d,%d sensor=%d,%d converted=%d,%d range=%d,%d..%d,%d samples=%u held=%u\n",
                          i+1,r.targetX,r.targetY,r.stableRawX,r.stableRawY,r.stableX,r.stableY,
                          r.minRawX,r.minRawY,r.maxRawX,r.maxRawY,r.samples,r.heldMs);
        }
    }
    Serial.println("CAL end");
    Serial.flush();
}

static void dumpTouchMapping() {
    const int16_t rawPoints[][2]={{0,0},{0,467},{467,0},{233,233},{228,427},{467,467}};
    auto touchConfig=M5.Display.touch()->config();
    auto panelConfig=M5.Display.panel()->config();
    Serial.printf("CALMAP panel=%ux%u touch_x=%u..%u touch_y=%u..%u\n",
                  panelConfig.panel_width,panelConfig.panel_height,
                  touchConfig.x_min,touchConfig.x_max,touchConfig.y_min,touchConfig.y_max);
    for(const auto& values:rawPoints) {
        lgfx::touch_point_t raw;
        raw.x=values[0]; raw.y=values[1];
        auto logical=raw;
        M5.Display.convertRawXY(&logical,1);
        Serial.printf("CALMAP raw=%d,%d logical=%d,%d\n",raw.x,raw.y,logical.x,logical.y);
    }
    Serial.flush();
}

static void printTouchAffineProfile() {
    bool candidateApplied=(_touchCalibration&&_touchCalibrationMode==TouchCalibrationMode::Verify)
        ||(_touchCalibrationMode==TouchCalibrationMode::Passed&&_touchCalibrationCandidateVerified);
    const float* current=candidateApplied
        ?_touchCalibrationCandidate.coefficients:_touchAffineActive;
    Serial.printf("CALPROFILE stored=%u candidate_applied=%u verified=%u current=%.8f,%.8f,%.8f,%.8f,%.8f,%.8f\n",
                  _touchAffineStored,candidateApplied,_touchCalibrationCandidateVerified,current[0],current[1],current[2],
                  current[3],current[4],current[5]);
    Serial.flush();
}

static void consumeWakeInput() {
    _buttonA.consume(M5.BtnA.isPressed());
    _buttonB.consume(M5.BtnB.isPressed());
    _touch.consume();
    resetUiPointer();
}

static void applyScreenPowerState(watchpower::ScreenState next) {
    if (_screenPowerState == next) return;
    const bool wasOff = _screenPowerState == watchpower::ScreenState::Off;
    _screenPowerState = next;
    _dozing = next != watchpower::ScreenState::Active;
    if (next == watchpower::ScreenState::Active) {
        power.wakeDisplay(settings.data().brightness);
        applySoundDemand();
        _faceNeedsClear = true;
        _buttonFeedbackFaceBaseValid = false;
        _buttonFeedbackHudMs = 0;
        _scrollMs = millis();
        _uiDirty = true;
        _listBandOnly = false;
        face.invalidate();
        _buttonFeedbackDirty = true;
    } else {
        applySoundDemand();
        _settingsCarousel.cancel();
        _timeScroll.cancel();
        _personalScroll.cancel();
        _editorScroll.cancel();
        if (next == watchpower::ScreenState::Dimmed) {
            const uint8_t level = watchpower::dimBrightnessLevel(
                settings.data().brightness, settings.data().dimBrightness);
            if (wasOff) {
                power.wakeDisplay(level);
                _faceNeedsClear = true;
                _buttonFeedbackFaceBaseValid = false;
                _scrollMs = millis();
                _uiDirty = true;
                _listBandOnly = false;
                face.invalidate();
            } else power.applyLevel(level);
        }
        else {
            _ambientCycle.setPaused(true,millis());
            power.sleepDisplay();
        }
    }
}

static void applySettings() {
    watchstrings::chinese()=settings.data().language!=0;
    settings.rebuildStyle();
    power.setIndicator(settings.data().indicator);
    applySoundDemand();
    settings.apply(face.bot());
    settings.apply(previewBot);
    face.bot().setName(settings.data().botName);
    previewBot.setName(settings.data().botName);
    face.bot().setGazeDirection((botux::BotUx::GazeDirection)settings.data().gaze);
    previewBot.setGazeDirection((botux::BotUx::GazeDirection)settings.data().gaze);
    face.setLayout(settings.data().swapLayout, settings.data().showDescription, settings.data().language);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    auto animation = (botux::BotUx::Animation)settings.data().animation;
    face.bot().setExpression(_manualPreset ? _manualCombo.expression : expression, 500);
    previewBot.setExpression(expression, 500);
    face.bot().setAnimation(_manualPreset ? _manualCombo.animation : animation);
    previewBot.setAnimation(animation);
    float amount = 0.25f + settings.data().motionAmount * 0.15f;
    float speed = 0.45f + settings.data().animationSpeed * 0.14f;
    face.bot().setMotionAmount(amount);
    previewBot.setMotionAmount(amount);
    face.bot().setAnimationSpeed(speed);
    previewBot.setAnimationSpeed(speed);
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    if (_screenPowerState == watchpower::ScreenState::Active)
        power.applyLevel(settings.data().brightness);
    else if (_screenPowerState == watchpower::ScreenState::Dimmed)
        power.applyLevel(watchpower::dimBrightnessLevel(
            settings.data().brightness, settings.data().dimBrightness));
    _uiDirty = true;
}

static void refreshPower(uint32_t now) {
    if (_powerKnown && now - _powerReadMs < 1000) return;
    _powerReadMs = now;
    bool wasCharging = _charging;
    power.update();
    _battery = power.batteryPct();
    _charging = power.charging();
    _externalPower = power.externalPower();
    watchClock.refresh();
    if (_powerKnown && _charging && !wasCharging) {
        _happyAcknowledgment.start(now, 1400);
        _statusPanelStartMs = now;
        _statusPanelUntilMs = now + 6000;
    }
    _powerKnown = true;
}

static void invalidateButtonFeedback() {
    _buttonFeedbackDirty = true;
}

static bool refreshButtonFeedback(uint32_t now) {
    _powerButtonWakePressed = false;
    if (now - _powerButtonReadMs >= 8) {
        _powerButtonReadMs = now;
        bool pressed = false;
        bool previousPressed = _powerButtonPressed;
        _powerButtonValid = power.readPowerButton(&pressed);
        _powerButtonPressed = _powerButtonValid && pressed;
        _powerButtonWakePressed = _powerButtonPressed && !previousPressed;
    }
    bool enabled = settings.data().buttonFeedback;
    bool changed = _buttonFeedback.sample(enabled && M5.BtnA.isPressed(),
                                          enabled && M5.BtnB.isPressed(),
                                          enabled && _powerButtonValid,
                                          enabled && _powerButtonPressed, now);
    changed = _buttonFeedback.advance(now) || changed;
    if (changed) {
        invalidateButtonFeedback();
    }
    return changed;
}

static void showStatusPanel(uint32_t now, uint32_t duration = 6000) {
    _statusPanelStartMs = now;
    _statusPanelUntilMs = now + duration;
    _uiDirty = true;
}

static uint8_t visibleBattery() {
    return _diagnosticBattery ? _diagnosticBatteryPct : _battery;
}

static float statusPanelProgress(uint32_t now) {
    if (!_statusPanelUntilMs) return 0.0f;
    if ((int32_t)(now - _statusPanelUntilMs) >= 0) {
        _statusPanelUntilMs = 0;
        return 0.0f;
    }
    uint32_t age = now - _statusPanelStartMs;
    if (age < 300) return age / 300.0f;
    uint32_t left = _statusPanelUntilMs - now;
    if (left < 260) return left / 260.0f;
    return 1.0f;
}

static bool directedGazeActive(uint32_t now) {
    return _gazeUntil&&(int32_t)(_gazeUntil-now)>0;
}

static botux::BotUx::Mood faceMood(uint32_t now) {
    if (_dozing) return botux::BotUx::Mood::Sleepy;
    if (_screen != Screen::Face) return botux::BotUx::Mood::Listening;
    if (directedGazeActive(now)) return botux::BotUx::Mood::Idle;
    if (_manualPreset) return _manualCombo.mood;
    if (_happyAcknowledgment.active(now)) return botux::BotUx::Mood::Happy;
    if (_battery <= kLowBattery) return botux::BotUx::Mood::Sleepy;
    return settings.data().expression == 0 ? _ambientMood : botux::BotUx::Mood::Idle;
}

static void showManualMood(botux::BotUx::Mood mood) {
    _gazeUntil=0; face.bot().clearGaze();
    _manualPreset=true;
    _manualCombo={mood,botux::BotUx::Expression::Auto,botux::BotUx::Animation::Auto};
    face.bot().setExpression(botux::BotUx::Expression::Auto,300);
    face.bot().setAnimation(botux::BotUx::Animation::Auto);
    face.bot().setTalking(mood==botux::BotUx::Mood::Speaking);
    _drawMood=(botux::BotUx::Mood)0xFF;
    face.invalidate(); clickSound();
}

static void enterSettings() {
    resetUiPointer();
    Screen prior = _screen;
    _screen = Screen::Settings;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _menu = MenuItem::Time;
    _settingsCarousel.configure(kMenuCount,watchcontrols::settingsCarousel().step);
    _settingsCarousel.select(0);
    _buttonNavigation=false;
    _uiDirty = true;
    confirmSound();
}

static void enterTimeSettings() {
    resetUiPointer();
    Screen prior=_screen;
    _screen=Screen::TimeSettings;
    pushTouchTrace(TouchTraceKind::Screen,millis(),0,0,(int16_t)prior,(int16_t)_screen);
    _editor=Editor::None;
    _timeItem=TimeItem::Time;
    _timeList.configure(kTimeCount,kVisibleRows);
    _timeList.select(0);
    _timeScroll.setBounds(kTimeCount*kMenuStep,watchcontrols::mainList().h);
    _timeScroll.setOffset(0);
    _buttonNavigation=false;
    _uiDirty=true;
    clickSound();
}

static void leaveTimeSettings(int cause=watchcontrols::None) {
    resetUiPointer();
    Screen prior=_screen;
    _screen=Screen::Settings;
    pushTouchTrace(TouchTraceKind::Screen,millis(),0,0,(int16_t)prior,(int16_t)_screen,
                   0,0,0,(uint8_t)cause);
    _editor=Editor::None;
    _uiDirty=true;
    clickSound();
}

static void enterPersonalize(Screen parent = Screen::Settings) {
    resetUiPointer();
    _personalParent = parent;
    Screen prior = _screen;
    _screen = Screen::Personalize;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _personal = PersonalItem::Expression;
    _personalList.configure(kPersonalCount, kVisibleRows);
    _personalList.select(0);
    _personalScroll.setBounds(kPersonalCount*kMenuStep,watchcontrols::mainList().h);
    _personalScroll.setOffset(0);
    _uiDirty = true;
    confirmSound();
}

static void leavePersonalize(int cause = watchcontrols::None) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = _personalParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
    _editor = Editor::None;
    _uiDirty = true;
    if (_screen == Screen::Face) {
        _faceNeedsClear = true;
        face.invalidate();
    }
    clickSound();
}

static void leaveSettings(int cause = watchcontrols::None) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = Screen::Face;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
    _editor = Editor::None;
    settings.save();
    _faceNeedsClear = true;
    face.invalidate();
    confirmSound();
}

static void enterEditor(Editor editor, Screen parent = Screen::Settings) {
    resetUiPointer();
    Screen prior = _screen;
    _screen = Screen::Editor;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = editor;
    _rtcSaveFailed = false;
    _originalSettings = settings.data();
    _originalManualPreset = _manualPreset; _originalManualCombo = _manualCombo;
    _editorParent = parent;
    _editField = 0;
    if(watchcontrols::editorUsesScrollList(editor)) {
        auto layout=watchcontrols::editorList(editor);
        _editorScroll.setBounds(watchcontrols::editorRowCount(editor)*layout.step,layout.h);
        _editorScroll.setOffset(0);
    } else _editorScroll.setBounds(0,0);
    _uiDirty = true;
    auto dt = watchClock.editorValue();
    _editHour = (uint8_t)dt.time.hours;
    _editMinute = (uint8_t)dt.time.minutes;
    _editYear = dt.date.year;
    _editMonth = (uint8_t)dt.date.month;
    _editDay = (uint8_t)dt.date.date;
    clickSound();
}

static void cancelEditor() {
    watchClock.cancelDraft();
    resetUiPointer();
    const bool powerEditor = _editor == Editor::Power;
    _manualPreset = _originalManualPreset; _manualCombo = _originalManualCombo;
    settings.data() = _originalSettings;
    applySettings();
    if(powerEditor) _idleScreenPolicy.wakeForUser(millis());
    Screen prior = _screen;
    _screen = _editorParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _editor = Editor::None;
    _uiDirty = true;
    clickSound();
}

static void returnHome() {
    watchClock.cancelDraft();
    if(_screen==Screen::Editor) {
        _manualPreset=_originalManualPreset; _manualCombo=_originalManualCombo;
        settings.data()=_originalSettings; applySettings();
    }
    _manualDozing=false;
    _idleScreenPolicy.wakeForUser(millis());
    applyScreenPowerState(watchpower::ScreenState::Active);
    _clockDoubleTap.reset(); consumeWakeInput();
    Screen prior=_screen;
    _screen=Screen::Face; _editor=Editor::None;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen);
    _faceNeedsClear=true; face.invalidate(); _uiDirty=true;
    playSound(ux::sound::Cue::Back, SoundChannel::Button);
}

static void saveEditor(int cause = watchcontrols::None) {
    resetUiPointer();
    if (_editor == Editor::Name) {
        _nameEditor.press(ux::NameEditor::Done);
        snprintf(settings.data().botName, sizeof(settings.data().botName), "%s", _nameEditor.text());
        applySettings();
    }
    if (_editor == Editor::Time || _editor == Editor::Date) {
        auto result=_editor==Editor::Time ? watchClock.setTime(_editHour,_editMinute)
            : watchClock.setDate(_editYear,_editMonth,_editDay);
        if(result==RtcClock::Save::Failed) {
            _rtcSaveFailed=true; _uiDirty=true;
            playSound(ux::sound::Cue::Error, SoundChannel::Alert);
            Serial.println("RTC save failed; editor retained");
            return;
        }
        if(result==RtcClock::Save::Pending) {
            // A lost clock needs both date and time, confirmed in either order.
            enterEditor(_editor==Editor::Time?Editor::Date:Editor::Time,_editorParent);
            return;
        }
        face.invalidate();
    } else {
        settings.save();
    }
    Screen prior = _screen;
    _screen = _editorParent;
    pushTouchTrace(TouchTraceKind::Screen, millis(), 0, 0, (int16_t)prior, (int16_t)_screen,
                   0, 0, 0, (uint8_t)cause);
    _editor = Editor::None;
    _uiDirty = true;
    confirmSound();
}

static void pokeBot() {
    face.bot().poke();
    _pokeReaction.start(millis(),1180);
    pokeSound();
}

static void startDoze() {
    const auto config=idlePowerConfig();
    if (!watchpower::policyApplies(config,_externalPower,
                                   powerClockReady(),powerClockHour())) return;
    _manualDozing = true;
    applyScreenPowerState(watchpower::ScreenState::Dimmed);
}

static void cycleExpression(int8_t delta, bool persist = false) {
    _manualPreset = false;
    face.bot().setTalking(false);
    settings.data().expression = (uint8_t)((settings.data().expression
        + Settings::EXPRESSION_COUNT + delta) % Settings::EXPRESSION_COUNT);
    auto expression = (botux::BotUx::Expression)settings.data().expression;
    face.bot().setAnimation((botux::BotUx::Animation)settings.data().animation);
    face.bot().setExpression(expression, 500);
    previewBot.setExpression(expression, 500);
    _uiDirty = true;
    if (persist) settings.save();
    clickSound();
}

static void updateMotion(uint32_t now) {
    if (now - _lastImuReadMs < 20) return;
    _lastImuReadMs = now;

    if (_dozing) {
        _motionFilter.reset();
        face.bot().setMotion(0.0f, 0.0f, 0.0f);
        return;
    }

    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    float gx = 0.0f, gy = 0.0f, gz = 0.0f;
    bool accelOk = false, gyroOk = false;
    if (settings.data().motion && M5.Imu.isEnabled()) {
        M5.Imu.update();
        accelOk = M5.Imu.getAccel(&ax, &ay, &az);
        gyroOk = M5.Imu.getGyro(&gx, &gy, &gz);
    }

    float gyroMagnitude = gyroOk ? sqrtf(gx * gx + gy * gy + gz * gz) : 0.0f;
    auto filtered = _motionFilter.update(accelOk ? ax : 0.0f, accelOk ? ay : 0.0f,
                                         gyroMagnitude, now);
    face.bot().setMotion(filtered.x, filtered.y, filtered.shake * 0.45f);
    previewBot.setMotion(filtered.x, filtered.y, filtered.shake * 0.45f);

    if (filtered.poke) {
        face.bot().poke();
        _pokeReaction.start(now,1180);
        pokeSound();
    }
}

static void changeEditorValue(int8_t delta) {
    if (_editor == Editor::Time) {
        if (_editField == 0) _editHour = (uint8_t)((_editHour + 24 + delta) % 24);
        else _editMinute = (uint8_t)((_editMinute + 60 + delta) % 60);
    } else if (_editor == Editor::Date) {
        if (_editField == 0) {
            _editMonth = (uint8_t)((_editMonth - 1 + 12 + delta) % 12 + 1);
        } else if (_editField == 1) {
            int maxDay = watchcalendar::daysInMonth(_editYear, _editMonth);
            _editDay = (uint8_t)((_editDay - 1 + maxDay + delta) % maxDay + 1);
        } else {
            _editYear = (int16_t)((_editYear - 2020 + 80 + delta) % 80 + 2020);
        }
        uint8_t maxDay = watchcalendar::daysInMonth(_editYear, _editMonth);
        if (_editDay > maxDay) _editDay = maxDay;
    } else if (_editor == Editor::Format) {
        if (_editField == 0) settings.data().hour24 = !settings.data().hour24;
        else settings.data().showSeconds = !settings.data().showSeconds;
        applySettings();
    } else if (_editor == Editor::Layout) {
        if (_editField == 0) settings.data().showDescription = !settings.data().showDescription;
        else settings.data().swapLayout = !settings.data().swapLayout;
        applySettings();
    } else if (_editor == Editor::Gaze) {
        settings.data().gaze = (settings.data().gaze + botux::BotUx::gazeDirectionCount() + delta) % botux::BotUx::gazeDirectionCount();
        applySettings();
    } else if (_editor == Editor::Language) {
        settings.data().language ^= 1; applySettings();
    } else if (_editor == Editor::Preview) {
        uint8_t* value = _editField == 0 ? &_previewMood : _editField == 1 ? &_previewExpression : &_previewAnimation;
        uint8_t count = _editField == 0 ? botux::BotUx::moodCount() : _editField == 1 ? botux::BotUx::expressionCount() : botux::BotUx::animationCount();
        *value = (*value + count + delta) % count;
    } else if (_editor == Editor::Expression) {
        cycleExpression(delta);
    } else if (_editor == Editor::Appearance) {
        if (_editField == 0) {
            settings.data().appearance = (uint8_t)((settings.data().appearance + Settings::APPEARANCE_COUNT + delta) % Settings::APPEARANCE_COUNT);
        } else {
            settings.data().eyeStyle = (uint8_t)((settings.data().eyeStyle + Settings::EYE_STYLE_COUNT + delta) % Settings::EYE_STYLE_COUNT);
        }
        applySettings();
    } else if (_editor == Editor::Motion) {
        if (_editField == 0) {
            _manualPreset = false;
            settings.data().animation = (uint8_t)((settings.data().animation
                + Settings::ANIMATION_COUNT + delta) % Settings::ANIMATION_COUNT);
        } else if (_editField == 1) {
            settings.data().motion = !settings.data().motion;
        } else if (_editField == 2) {
            int level = settings.data().motionAmount + delta;
            settings.data().motionAmount = (uint8_t)(level < 1 ? 1 : level > 5 ? 5 : level);
        } else {
            int level = settings.data().animationSpeed + delta;
            settings.data().animationSpeed = (uint8_t)(level < 1 ? 1 : level > 5 ? 5 : level);
        }
        applySettings();
    } else if (_editor == Editor::Display) {
        if (_editField == 0) {
            int level = settings.data().brightness + delta;
            settings.data().brightness = (uint8_t)(level < Settings::BRIGHTNESS_MIN
                ? Settings::BRIGHTNESS_MIN : level > Settings::BRIGHTNESS_MAX
                ? Settings::BRIGHTNESS_MAX : level);
            if(settings.data().dimBrightness>settings.data().brightness)
                settings.data().dimBrightness=settings.data().brightness;
        } else if (_editField == 1) {
            settings.data().theme = (uint8_t)((settings.data().theme + Settings::THEME_COUNT + delta) % Settings::THEME_COUNT);
        } else if(_editField==2) {
            settings.data().indicator=!settings.data().indicator;
        } else if(_editField==3) {
            settings.data().buttonFeedback=!settings.data().buttonFeedback;
        } else if(_editField==4) {
            settings.data().showDescription=!settings.data().showDescription;
        } else {
            settings.data().swapLayout=!settings.data().swapLayout;
        }
        applySettings();
    } else if (_editor == Editor::Sound) {
        if(_editField==0) settings.data().sound=!settings.data().sound;
        else if(_editField==1) settings.data().startupSound=!settings.data().startupSound;
        else if(_editField==2) settings.data().buttonSound=!settings.data().buttonSound;
        else settings.data().alertSound=!settings.data().alertSound;
        applySettings();
    } else if (_editor == Editor::Power) {
        if(_editField==0) settings.data().powerSaveEnabled=!settings.data().powerSaveEnabled;
        else if(_editField==1) {
            int level=settings.data().dimBrightness+delta;
            int maxLevel=settings.data().brightness;
            settings.data().dimBrightness=(uint8_t)(level<Settings::BRIGHTNESS_MIN
                ?Settings::BRIGHTNESS_MIN:level>maxLevel?maxLevel:level);
        } else if(_editField==2) {
            settings.data().dimTimeout=(uint8_t)((settings.data().dimTimeout
                +Settings::TIMEOUT_COUNT+delta)%Settings::TIMEOUT_COUNT);
        } else if(_editField==3) {
            settings.data().screenOffTimeout=(uint8_t)((settings.data().screenOffTimeout
                +Settings::TIMEOUT_COUNT+delta)%Settings::TIMEOUT_COUNT);
        } else if(_editField==4) {
            settings.data().wakeMode=(uint8_t)((settings.data().wakeMode
                +Settings::WAKE_MODE_COUNT+delta)%Settings::WAKE_MODE_COUNT);
        } else if(_editField==5) {
            settings.data().keepAwakeWhileCharging=!settings.data().keepAwakeWhileCharging;
        } else if(_editField==6) {
            settings.data().forcedSleepEnabled=!settings.data().forcedSleepEnabled;
        } else if(_editField==7) {
            settings.data().forcedSleepStartHour=(uint8_t)((settings.data().forcedSleepStartHour+24+delta)%24);
        } else {
            settings.data().forcedSleepEndHour=(uint8_t)((settings.data().forcedSleepEndHour+24+delta)%24);
        }
        applySettings();
        _idleScreenPolicy.wakeForUser(millis());
    }
    clickSound();
}

static void selectMenuItem() {
    switch (_menu) {
        case MenuItem::Time:        enterTimeSettings(); break;
        case MenuItem::Personalize: enterPersonalize(); break;
        case MenuItem::Display:     enterEditor(Editor::Display); break;
        case MenuItem::Sound:       enterEditor(Editor::Sound); break;
        case MenuItem::Power:       enterEditor(Editor::Power); break;
        case MenuItem::Done:        leaveSettings(); break;
        default: break;
    }
}

static void selectTimeItem() {
    switch(_timeItem) {
        case TimeItem::Time: enterEditor(Editor::Time,Screen::TimeSettings); break;
        case TimeItem::Date: enterEditor(Editor::Date,Screen::TimeSettings); break;
        case TimeItem::Format: enterEditor(Editor::Format,Screen::TimeSettings); break;
        case TimeItem::Back: leaveTimeSettings(); break;
        default: break;
    }
}

static void selectPersonalItem() {
    switch (_personal) {
        case PersonalItem::Expression: enterEditor(Editor::Expression, Screen::Personalize); break;
        case PersonalItem::Action:     enterEditor(Editor::Motion, Screen::Personalize); break;
        case PersonalItem::Appearance: enterEditor(Editor::Appearance, Screen::Personalize); break;
        case PersonalItem::Color:      enterEditor(Editor::Color, Screen::Personalize); break;
        case PersonalItem::Name: enterEditor(Editor::Name, Screen::Personalize); _nameEditor.begin(settings.data().botName); break;
        case PersonalItem::Language: enterEditor(Editor::Language, Screen::Personalize); break;
        case PersonalItem::Preview: enterEditor(Editor::Preview, Screen::Personalize); _previewMood = _previewExpression = _previewAnimation = 0; break;
        case PersonalItem::Gaze: enterEditor(Editor::Gaze, Screen::Personalize); break;
        case PersonalItem::Intensity:  enterEditor(Editor::Motion, Screen::Personalize); _editField = 2; break;
        case PersonalItem::Speed:      enterEditor(Editor::Motion, Screen::Personalize); _editField = 3; break;
        case PersonalItem::Back:       leavePersonalize(); break;
        default: break;
    }
}

static void handleFaceInput(Gesture gesture) {
    if (gesture == Gesture::Tap) {
        int16_t releaseOffset=watchedge::batteryToothOffset(statusPanelProgress(millis()));
        bool batteryTap=_statusPanelTouchVisible
            && watchedge::batteryToothContains(_touch.startX(),_touch.startY(),_statusPanelTouchOffset)
            && watchedge::batteryToothContains(_tapX,_tapY,releaseOffset);
        _statusPanelTouchVisible=false;
        if (_statusPanelUntilMs && batteryTap) {
            _statusPanelUntilMs = 0;
            _clockDoubleTap.reset();
            return;
        }
        const int clockY=settings.data().swapLayout?20:376;
        if(_clockDoubleTap.tap(tapHit(90,clockY,286,70),millis())) { enterSettings(); return; }
        directFaceGaze(_tapX,_tapY,millis());
    } else if (gesture != Gesture::None) {
        _clockDoubleTap.reset();
        if (gesture == Gesture::SwipeDown && _touch.startY() <= 20)
            showStatusPanel(millis());
    }
}

static void updateColorPicker(int16_t x, int16_t y) {
    if (_colorDrag == 1) {
        auto pad=watchcontrols::colorPadBounds();
        int16_t right=pad.x+pad.w-1,bottom=pad.y+pad.h-1;
        if(x<pad.x) x=pad.x; if(x>right) x=right;
        if(y<pad.y) y=pad.y; if(y>bottom) y=bottom;
        settings.data().colorSat=(uint8_t)((x-pad.x)*100/(pad.w-1));
        settings.data().colorValue=(uint8_t)((bottom-y)*100/(pad.h-1));
    } else if (_colorDrag == 2) {
        auto hue=watchcontrols::hueBarBounds();
        int16_t bottom=hue.y+hue.h-1;
        if(y<hue.y) y=hue.y; if(y>bottom) y=bottom;
        settings.data().colorHue=(uint16_t)((y-hue.y)*359/(hue.h-1));
    } else return;
    settings.data().customColor = true;
    applySettings();
}

static void activateUiControl(int target) {
    using namespace watchcontrols;
    if(_screen==Screen::Settings || _screen==Screen::TimeSettings || _screen==Screen::Personalize) {
        if(target==Done) {
            if(_screen==Screen::Personalize) leavePersonalize(target);
            else if(_screen==Screen::TimeSettings) leaveTimeSettings(target);
            else leaveSettings(target);
            return;
        }
    }
    if (_screen == Screen::Settings && target >= MenuRow) {
        _settingsCarousel.select((uint8_t)(target-MenuRow));
        _menu=(MenuItem)_settingsCarousel.selected(); selectMenuItem(); return;
    }
    if(_screen==Screen::Settings
        &&(target==CarouselPrevious||target==CarouselNext)) {
        _buttonNavigation=true;
        _settingsCarousel.moveSelection(target==CarouselPrevious?-1:1);
        _menu=(MenuItem)_settingsCarousel.selected();
        _uiDirty=true;
        clickSound();
        return;
    }
    if (_screen == Screen::TimeSettings && target >= TimeRow) {
        _timeList.select(target-TimeRow); _timeItem=(TimeItem)_timeList.selected(); selectTimeItem(); return;
    }
    if (_screen == Screen::Personalize && target >= PersonalRow) {
        _personalList.select(target-PersonalRow); _personal=(PersonalItem)_personalList.selected(); selectPersonalItem(); return;
    }
    if (_screen != Screen::Editor) return;
    if (target == Done) { saveEditor(target); return; }
    int field=target-First;
    if (_editor == Editor::Name && target >= NameKey && target < NameKey+30) {
        bool done=_nameEditor.press(target-NameKey); clickSound(); _uiDirty=true;
        if(done) saveEditor(target);
    } else if (_editor == Editor::Language) {
        settings.data().language=field; applySettings(); clickSound();
    } else if (_editor == Editor::Layout || _editor == Editor::Preview || _editor == Editor::Gaze
            || _editor == Editor::Appearance || _editor == Editor::Motion || _editor == Editor::Display
            || _editor == Editor::Sound || _editor == Editor::Power) {
        _editField=field; changeEditorValue(_tapX<233?-1:1);
    } else if (_editor == Editor::Color && target==First) {
        settings.data().customColor=false; applySettings(); clickSound();
    } else if (_editor == Editor::Time || _editor == Editor::Date) {
        _editField=field/3;
        if(field%3==2) { _uiDirty=true; clickSound(); }
        else changeEditorValue(field%3==0?-1:1);
    } else if (_editor == Editor::Format) {
        if(field<2) settings.data().hour24=field==1;
        else settings.data().showSeconds=!settings.data().showSeconds;
        applySettings(); clickSound();
    } else if (_editor == Editor::Expression) {
        if(field<2) cycleExpression(field==0?-1:1);
        else { previewBot.poke(); pokeSound(); }
    }
}

static int exactPressedTarget() {
    if(!_pointer.pressed()) return watchcontrols::None;
    float offset=_screen==Screen::Settings?_settingsCarousel.offset():
        _screen==Screen::TimeSettings?_timeScroll.offset():
        _screen==Screen::Personalize?_personalScroll.offset():
        _screen==Screen::Editor&&watchcontrols::editorUsesScrollList(_editor)?_editorScroll.offset():0;
    auto target=watchcontrols::at(_screen,_editor,offset,_pointer.x(),_pointer.y());
    return target.id==_capturedTarget?target.id:watchcontrols::None;
}

// Hardware and diagnostic input share this path; one session owns each gesture.
static void handleUiPointer(bool down, bool held, bool up, int x, int y, uint32_t now) {
    if (_screen==Screen::Face) return;
    ux::ScrollModel* scroll=_screen==Screen::TimeSettings?&_timeScroll:
        _screen==Screen::Personalize?&_personalScroll:
        _screen==Screen::Editor&&watchcontrols::editorUsesScrollList(_editor)?&_editorScroll:nullptr;
    const auto scrollLayout=_screen==Screen::Editor?watchcontrols::editorList(_editor):watchcontrols::mainList();
    int oldPressed=exactPressedTarget();
    if(down) {
        _buttonNavigation=false; _uiDirty=true; _listBandOnly=false;
        if(scroll) scroll->cancel(); // Stop inertia before capturing the visible row.
        float offset=_screen==Screen::Settings?_settingsCarousel.offset():scroll?scroll->offset():0;
        auto target=watchcontrols::at(_screen,_editor,offset,x,y);
        _capturedTarget=target.id;
        const auto carousel=watchcontrols::settingsCarousel();
        const bool inCarousel=_screen==Screen::Settings&&x>=carousel.x&&x<carousel.x+carousel.w
            &&y>=carousel.y&&y<carousel.y+carousel.h;
        _pointer.begin(target.id,target.bounds,x,y,now,
                       inCarousel||(scroll&&y>=scrollLayout.y&&y<scrollLayout.y+scrollLayout.h));
        if(inCarousel)
            _settingsCarousel.begin(x,now);
        pushTouchTrace(TouchTraceKind::Down, now, x, y, target.id);
        _colorDrag=target.id==watchcontrols::ColorPad?1:target.id==watchcontrols::HueBar?2:0;
    }
    if(_screen==Screen::Settings&&_settingsCarousel.active()&&(held||up)) {
        bool wasDragging=_settingsCarousel.dragging();
        float before=_settingsCarousel.offset();
        if(wasDragging||watchcontrols::carouselDragIntent(
                x-_pointer.startX(),y-_pointer.startY()))
            _settingsCarousel.move(x,now);
        if(!wasDragging&&_settingsCarousel.dragging()) {
            _pointer.cancel();
            pushTouchTrace(TouchTraceKind::Scroll,now,x,y,_capturedTarget,
                           watchcontrols::None,now-_pointer.startedAt());
        }
        if(before!=_settingsCarousel.offset()) markListMoved();
        if(up) {
            bool dragged=_settingsCarousel.end(now);
            _menu=(MenuItem)_settingsCarousel.selected();
            if(dragged) {
                pushTouchTrace(TouchTraceKind::End,now,x,y,_capturedTarget,
                               watchcontrols::None,now-_pointer.startedAt(),0,0,
                               (uint8_t)TouchEndReason::Scrolled);
                _capturedTarget=watchcontrols::None;
                _tapX=x; _tapY=y;
                _uiDirty=true; _listBandOnly=true;
                return;
            }
        }
    }
    if(_pointer.active() && (held||up)) {
        bool wasScrolling=_pointer.scrolling();
        _pointer.move(x,y);
        if(scroll && _pointer.scrolling()) {
            if(!wasScrolling) {
                scroll->begin(_pointer.startY(),_pointer.startedAt());
                pushTouchTrace(TouchTraceKind::Scroll, now, x, y, _capturedTarget,
                               watchcontrols::None, now-_pointer.startedAt());
            }
            float before=scroll->offset(); scroll->move(y,now);
            if(before!=scroll->offset()) markListMoved();
        }
        if(_colorDrag) updateColorPicker(x,y);
    }
    if(up && _pointer.active()) {
        bool dragged=_pointer.scrolling();
        auto releasedBounds=_pointer.bounds();
        float offset=_screen==Screen::Settings?_settingsCarousel.offset():scroll?scroll->offset():0;
        auto releasedTarget=watchcontrols::at(_screen,_editor,offset,x,y);
        uint32_t elapsed=now-_pointer.startedAt();
        int captured=_capturedTarget;
        TouchEndReason reason=TouchEndReason::Accepted;
        if(captured==watchcontrols::None) reason=TouchEndReason::NoTarget;
        else if(dragged) reason=TouchEndReason::Scrolled;
        else if(releasedTarget.id!=captured) reason=TouchEndReason::TargetChanged;
        else if(elapsed>1000) reason=TouchEndReason::Timeout;
        if(!dragged&&releasedTarget.id!=_capturedTarget) _pointer.cancel();
        int clicked=_pointer.end(x,y,now);
        if(clicked<0&&reason==TouchEndReason::Accepted) reason=TouchEndReason::Outside;
        pushTouchTrace(TouchTraceKind::End, now, x, y, captured, releasedTarget.id,
                       elapsed, 0, 0, (uint8_t)reason);
        if(clicked>=0) { _clickRect=releasedBounds; _clickUntil=now+90; }
        if(scroll && dragged) scroll->end(now);
        _colorDrag=0;
        _capturedTarget=watchcontrols::None;
        _tapX=x; _tapY=y;
        if(clicked>=0 && clicked<watchcontrols::ColorPad) activateUiControl(clicked);
    }
    int pressed=exactPressedTarget();
    _namePressedKey=pressed>=watchcontrols::NameKey&&pressed<watchcontrols::NameKey+30?pressed-watchcontrols::NameKey:-1;
    if(pressed!=oldPressed) { _uiDirty=true; _listBandOnly=false; }
}

static void handleInputs(uint32_t now) {
    bool contact=_contact.isPressed(), acquired=false; int32_t x=_contact.x,y=_contact.y;
    _traceSampleSensorKnown=false;
    if(_diagnosticContact) {
        contact=_diagnosticDown; x=_diagnosticX; y=_diagnosticY;
    }
    else if(_screenPowerState==watchpower::ScreenState::Off
            &&settings.data().wakeMode==Settings::WAKE_KEYS_ONLY) {
        contact=false;
    }
    else if(now-_contactReadMs>=8) {
        uint32_t previousReadMs=_contactReadMs;
        _contactReadMs=now;
        uint32_t readStartUs=micros();
        acquired=true;
        if(_touchCalibration||_touchTraceEnabled) {
            lgfx::touch_point_t point;
            contact=M5.Display.getTouchRaw(&point,1)!=0;
            if(contact) {
                _touchRawX=point.x; _touchRawY=point.y;
                if(_touchTraceEnabled) {
                    _traceSampleSensorKnown=_traceLastSensorKnown=true;
                    _traceSampleSensorX=_traceLastSensorX=point.x;
                    _traceSampleSensorY=_traceLastSensorY=point.y;
                    _traceLastSensorAcquisition=_traceAcquireCount+1;
                }
                M5.Display.convertRawXY(&point,1);
                x=point.x; y=point.y;
            }
        } else contact=M5.Display.getTouch(&x,&y);
        uint32_t readUs=micros()-readStartUs;
        uint32_t sampleGapMs=previousReadMs?now-previousReadMs:0;
        if(_touchTraceEnabled) {
            ++_traceAcquireCount;
            if(sampleGapMs>_traceMaxSampleGapMs) _traceMaxSampleGapMs=traceClamp16(sampleGapMs);
            if(readUs>_traceMaxReadUs) _traceMaxReadUs=traceClamp16(readUs);
            bool heldCheckpoint=contact&&(now-_traceLastHeldMs>=100);
            // M5GFX's StopWatch board setup maps CST820 INT to GPIO13. Low means
            // the controller signalled an event; it does not identify an I2C error.
            bool intLow=digitalRead(13)==LOW;
            bool delayed=sampleGapMs>=24;
            if(watchtrace::keepAcquisition(_traceRawKnown,contact,_traceRawContact,
                                           heldCheckpoint,delayed)) {
                uint8_t flags=(contact?1:0)|(intLow?2:0);
                pushTouchTrace(TouchTraceKind::Sample,now,x,y,watchcontrols::None,
                               watchcontrols::None,0,sampleGapMs,readUs,flags);
                if(contact) _traceLastHeldMs=now;
            }
            _traceRawKnown=true;
            _traceRawContact=contact;
        }
    }
    _contact.sample(contact,x,y);
    const auto& touch=_contact;
    if(_touchCalibration) {
        if(_touchCalibrationMode==TouchCalibrationMode::Direction
           &&_touchCalibrationDirectionGate) {
            if(M5.BtnA.wasClicked()||M5.BtnB.wasClicked()) {
                _touchCalibrationDirectionGate=false;
                _touchCalibrationAwaitRelease=true;
                _touchCalibrationReleaseSamples=0;
                _touchCalibrationLastContactMs=now;
                _contact.sample(false,0,0);
                _uiDirty=true;
                _listBandOnly=false;
            }
            return;
        }
        sampleTouchCalibration(contact,acquired,(int16_t)x,(int16_t)y,now);
        return;
    }
    bool powerClicked=M5.BtnPWR.wasClicked();
    _gestureActive = M5.BtnA.isPressed() || M5.BtnB.isPressed()
        || _powerButtonPressed || touch.wasPressed() || touch.isPressed();
    auto wakeGate=_wakeInputGate.update(now,_gestureActive,
                                        _powerButtonPressed,powerClicked);
    if(wakeGate.blockControls) {
        _idleScreenPolicy.wakeForUser(now);
        consumeWakeInput();
        return;
    }
    bool wakePressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed()
        || _powerButtonWakePressed || M5.BtnPWR.wasPressed() || powerClicked
        || touch.wasPressed();
    bool wasDozing=_screenPowerState!=watchpower::ScreenState::Active;
    const auto idleConfig=idlePowerConfig();
    const bool clockReady=powerClockReady();
    const uint8_t clockHour=powerClockHour();
    if(!watchpower::policyApplies(idleConfig,_externalPower,
                                  clockReady,clockHour)) {
        _manualDozing=false;
        _idleScreenPolicy.update(now,_externalPower,_gestureActive,
                                 clockReady,clockHour,idleConfig);
        applyScreenPowerState(watchpower::ScreenState::Active);
        if(wasDozing&&(_gestureActive||wakePressed)) {
            bool pendingPowerClick=!powerClicked
                &&(_powerButtonWakePressed||M5.BtnPWR.wasPressed());
            _wakeInputGate.beginWake(pendingPowerClick);
            consumeWakeInput();
            return;
        }
    } else if(wasDozing) {
        if(watchpower::shouldWakeAndConsume(_screenPowerState,wakePressed)) {
            _manualDozing=false;
            _idleScreenPolicy.wakeForUser(now);
            applyScreenPowerState(watchpower::ScreenState::Active);
            bool pendingPowerClick=!powerClicked
                &&(_powerButtonWakePressed||M5.BtnPWR.wasPressed());
            _wakeInputGate.beginWake(pendingPowerClick);
            consumeWakeInput();
        } else {
            auto automatic=_idleScreenPolicy.update(now,_externalPower,false,
                clockReady,clockHour,idleConfig);
            auto next=_manualDozing&&automatic!=watchpower::ScreenState::Off
                ?watchpower::ScreenState::Dimmed:automatic;
            applyScreenPowerState(next);
        }
        return;
    } else {
        auto next=_idleScreenPolicy.update(now,_externalPower,_gestureActive,
            clockReady,clockHour,idleConfig);
        applyScreenPowerState(next);
        if(next!=watchpower::ScreenState::Active) return;
    }
    if(powerClicked) {
        if(wakeGate.consumedPowerClick) return;
        returnHome(); return;
    }

    if (_screen == Screen::Face) {
        bool entered = _settingsChord.poll(M5.BtnA.isPressed(), M5.BtnB.isPressed(), now);
        if (_settingsChord.tracking()) {
            _bClick.cancel();
            _buttonA.consume(M5.BtnA.isPressed());
            _buttonB.consume(M5.BtnB.isPressed());
            if (entered) enterSettings();
            return;
        }
    }

    Gesture ga = _buttonA.poll(M5.BtnA.wasPressed(), M5.BtnA.isPressed(), M5.BtnA.wasReleased(), now);
    Gesture gb = _buttonB.poll(M5.BtnB.wasPressed(), M5.BtnB.isPressed(), M5.BtnB.wasReleased(), now);
    Gesture gt = Gesture::None;
    if (_screen == Screen::Face) {
        if(touch.wasPressed()) {
            _faceTracking=true;
            _statusPanelTouchOffset=watchedge::batteryToothOffset(statusPanelProgress(now));
            _statusPanelTouchVisible=_statusPanelUntilMs
                && watchedge::batteryToothContains(touch.x,touch.y,_statusPanelTouchOffset);
        }
        if(touch.isPressed()&&_faceTracking&&now-_gazeTouchMs>=33) {
            _gazeTouchMs=now; directFaceGaze(touch.x,touch.y,now);
        }
        if(touch.wasReleased()) _faceTracking=false;
        gt = _touch.poll(touch.wasPressed(),touch.isPressed(),touch.wasReleased(),touch.x,touch.y,now,3000);
        if(gt==Gesture::Tap) { _tapX=_touch.tapX(); _tapY=_touch.tapY(); }
        else if(gt!=Gesture::None) { _tapX=touch.x; _tapY=touch.y; }
    } else handleUiPointer(touch.wasPressed(),touch.isPressed(),touch.wasReleased(),touch.x,touch.y,now);

    if (_screen == Screen::Face) {
        if (ga == Gesture::Tap) {
            _bClick.cancel();
            uint8_t current=(uint8_t)(_manualPreset?_manualCombo.mood:face.bot().mood());
            auto mood=(botux::BotUx::Mood)_moodDeck.random(current);
            showManualMood(mood);
        }
        if (gb == Gesture::Long) { _bClick.cancel(); startDoze(); }
        auto click=_bClick.poll(gb==Gesture::Tap,now);
        if(click==watchinteraction::SingleDoubleClick::Event::Single) {
            uint8_t current=(uint8_t)(_manualPreset?_manualCombo.mood:face.bot().mood());
            auto mood=(botux::BotUx::Mood)_moodDeck.next(current);
            showManualMood(mood);
        } else if(click==watchinteraction::SingleDoubleClick::Event::Double) {
            _manualPreset=false;
            settings.data().expression=settings.data().animation=0;
            face.bot().resetToIdle(); face.bot().setTalking(false);
            _ambientMood=_drawMood=botux::BotUx::Mood::Idle;
            _ambientCycle.restart(now);
            face.invalidate(); confirmSound();
        }
        if (gt != Gesture::None) handleFaceInput(gt);
        return;
    }
    _bClick.cancel();

    if (_screen == Screen::Settings) {
        if (ga == Gesture::Tap) { _buttonNavigation=true; selectMenuItem(); }
        else if (ga == Gesture::Long) leaveSettings();
        if (gb == Gesture::Tap) {
            _buttonNavigation=true;
            _settingsCarousel.moveSelection(1);
            _menu = (MenuItem)_settingsCarousel.selected();
            _uiDirty = true;
            clickSound();
        }
        return;
    }

    if (_screen == Screen::TimeSettings) {
        if(ga==Gesture::Tap) { _buttonNavigation=true; selectTimeItem(); }
        else if(ga==Gesture::Long) leaveTimeSettings();
        if(gb==Gesture::Tap) {
            _buttonNavigation=true;
            _timeList.move(1);
            _timeItem=(TimeItem)_timeList.selected();
            revealRow(_timeScroll,(uint8_t)_timeItem);
            _uiDirty=true;
            clickSound();
        }
        return;
    }

    if (_screen == Screen::Personalize) {
        if (ga == Gesture::Tap) { _buttonNavigation=true; selectPersonalItem(); }
        else if (ga == Gesture::Long) leavePersonalize();
        if (gb == Gesture::Tap) {
            _buttonNavigation=true;
            _personalList.move(1);
            _personal = (PersonalItem)_personalList.selected();
            revealRow(_personalScroll, (uint8_t)_personal);
            _uiDirty = true;
        }
        return;
    }

    if (ga == Gesture::Tap) { _buttonNavigation=true; changeEditorValue(-1); }
    else if (ga == Gesture::Long) cancelEditor();
    if (gb == Gesture::Tap) { _buttonNavigation=true; changeEditorValue(1); }
    else if (gb == Gesture::Long) saveEditor();

}

static bool pointerFeedback(ux::Rect& r) {
    if(exactPressedTarget()!=watchcontrols::None) { r=_pointer.bounds(); return true; }
    if(_clickUntil && (int32_t)(millis()-_clickUntil)<0) { r=_clickRect; return true; }
    return false;
}
static bool pressedOver(int x,int y,int w,int h) {
    ux::Rect r; if(!pointerFeedback(r)) return false;
    return hit(r.x+r.w/2,r.y+r.h/2,x,y,w,h);
}
static void drawPointerFeedback() {
    // Each control paints feedback in its own exact hit geometry.
}

static void drawPill(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* label, bool selected, bool accentFill = false) {
    uint16_t fill = accentFill ? settings.style().accentColor : settings.panel();
    if(pressedOver(x,y,w,h)) fill=ux::blend565(fill,settings.ink(),48);
    int16_t radius = ((w < h) ? w : h) / 2;
    ux::roundRect(canvas, x, y, w, h, radius, fill);
    ux::strokeRoundRect(canvas, x, y, w, h, radius,
                         selected ? settings.style().accentColor : settings.muted());
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(accentFill ? settings.style().bgColor : settings.ink());
    watchText(canvas, label, x + w / 2, y + h / 2);
}

static void drawTitle(const char* title) {
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.ink());
    watchText(canvas, title, kW / 2, 48);
}

static void drawFooter();
static void drawSettingsFooter();

class SettingsCarouselView {
public:
    explicit SettingsCarouselView(M5Canvas& target):_target(target) {}
    uint16_t background() const { return 0; }
    uint16_t ink() const { return settings.ink(); }
    uint16_t muted() const { return settings.muted(); }
    bool pressed(ux::Rect bounds) const {
        return pressedOver(bounds.x,bounds.y,bounds.w,bounds.h);
    }
    void large(const char* text,int16_t x,int16_t y,uint16_t color) {
        _target.setTextDatum(middle_center);
        _target.setFont(&fonts::FreeSansBold12pt7b);
        _target.setTextSize(1.0f);
        _target.setTextColor(color);
        watchText(_target,text,x,y);
    }
private:
    M5Canvas& _target;
};

static void drawSettingsList(bool chrome = true) {
    if (chrome) drawTitle("SETTINGS");
    const auto layout=watchcontrols::settingsCarousel();
    const auto band=watchcontrols::settingsCarouselMotionBand();
    const auto clip=chrome?ux::Rect{layout.x,layout.y,layout.w,layout.h}:band;
    canvas.setClipRect(clip.x,clip.y,clip.w,clip.h);
    SettingsCarouselView view(canvas);
    watchsettingsdirect::CanvasWriter<M5Canvas> iconCanvas(canvas);
    watchsettingscarousel::draw(canvas,iconCanvas,view,_settingsCarousel.offset(),
        _settingsCarousel.selected(),kMenuLabels);
    canvas.clearClipRect();
    if (chrome) drawSettingsFooter();
}

static void drawTimeSettingsList(bool chrome=true) {
    if(chrome) drawTitle("TIME SETTINGS");
    const auto layout=watchcontrols::mainList();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    for(uint8_t i=0;i<kTimeCount;++i) {
        auto row=watchcontrols::rowBounds(layout,i,_timeScroll.offset());
        int16_t cy=row.y+row.h/2;
        if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) continue;
        bool selected=_buttonNavigation&&i==(uint8_t)_timeItem;
        bool pressed=pressedOver(row.x,row.y,row.w,row.h);
        ux::roundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,
            pressed?ux::blend565(settings.panel(),settings.style().accentColor,55)
                   :selected?settings.panel():settings.style().bgColor);
        if(selected) ux::strokeRoundRect(canvas,row.x,row.y,row.w,row.h,
                                         layout.radius,settings.style().accentColor);
        canvas.setTextDatum(middle_left);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextColor(selected?settings.style().accentColor:settings.ink());
        watchText(canvas,kTimeLabels[i],84,cy);
        char value[32]={};
        switch((TimeItem)i) {
            case TimeItem::Time:
                if(watchClock.hasTime()) snprintf(value,sizeof(value),"%02u:%02u",
                    watchClock.value().time.hours,watchClock.value().time.minutes);
                else snprintf(value,sizeof(value),"--:--");
                break;
            case TimeItem::Date:
                if(watchClock.hasTime()) snprintf(value,sizeof(value),"%04d/%02u/%02u",
                    watchClock.value().date.year,watchClock.value().date.month,
                    watchClock.value().date.date);
                else snprintf(value,sizeof(value),"----/--/--");
                break;
            case TimeItem::Format: snprintf(value,sizeof(value),settings.data().hour24?"24 H":"12 H"); break;
            default: break;
        }
        if(value[0]) {
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            watchText(canvas,value,382,cy);
        }
    }
    canvas.clearClipRect();
    if(chrome) drawFooter();
}

static void drawPersonalizeList(bool chrome = true) {
    if (chrome) drawTitle("BOT PERSONALITY");
    const auto layout=watchcontrols::mainList();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    for (uint8_t i = 0; i < kPersonalCount; ++i) {
        auto row=watchcontrols::rowBounds(layout,i,_personalScroll.offset());
        int16_t cy=row.y+row.h/2;
        if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) continue;
        bool selected=_buttonNavigation&&i==(uint8_t)_personal;
        bool pressed=pressedOver(row.x,row.y,row.w,row.h);
        ux::roundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,pressed?ux::blend565(settings.panel(),settings.style().accentColor,55):selected?settings.panel():settings.style().bgColor);
        if(selected) ux::strokeRoundRect(canvas,row.x,row.y,row.w,row.h,layout.radius,settings.style().accentColor);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.setTextDatum(middle_left);
        canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
        watchText(canvas,kPersonalLabels[i],84,cy);
        char value[40] = {};
        switch ((PersonalItem)i) {
            case PersonalItem::Expression: snprintf(value, sizeof(value), "%s", Settings::expressionName(settings.data().expression)); break;
            case PersonalItem::Action: snprintf(value, sizeof(value), "%s", Settings::animationName(settings.data().animation)); break;
            case PersonalItem::Appearance: snprintf(value, sizeof(value), "%s", Settings::appearanceName(settings.data().appearance)); break;
            case PersonalItem::Color: snprintf(value, sizeof(value), settings.data().customColor ? "CUSTOM" : "THEME"); break;
            case PersonalItem::Name: snprintf(value, sizeof(value), "%s", settings.data().botName); break;
            case PersonalItem::Language: snprintf(value, sizeof(value), settings.data().language ? "中文" : "English"); break;
            case PersonalItem::Preview: snprintf(value,sizeof(value),"%u",
                (unsigned)botux::BotUx::moodCount()*botux::BotUx::expressionCount()*botux::BotUx::animationCount()); break;
            case PersonalItem::Gaze: snprintf(value,sizeof(value),"%s",botux::BotUx::gazeDirectionName((botux::BotUx::GazeDirection)settings.data().gaze,settings.data().language?botux::BotUx::Language::Chinese:botux::BotUx::Language::English)); break;
            case PersonalItem::Intensity: snprintf(value, sizeof(value), "%u / 5", settings.data().motionAmount); break;
            case PersonalItem::Speed: snprintf(value, sizeof(value), "%u / 5", settings.data().animationSpeed); break;
            default: break;
        }
        if (value[0]) {
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.setTextDatum(middle_right);
            canvas.setTextColor(settings.muted());
            if(i==(uint8_t)PersonalItem::Name) watchEllipsizedText(canvas,value,382,cy,214);
            else watchText(canvas,value,382,cy);
        }
    }
    canvas.clearClipRect();
    if (chrome) drawFooter();
}

static void drawFooter() {
    auto bounds=watchcontrols::doneBounds();
    uint16_t fill=settings.style().accentColor;
    if(pressedOver(bounds.x,bounds.y,bounds.w,bounds.h))
        fill=ux::blend565(fill,settings.ink(),48);
    for(int16_t y=bounds.y;y<bounds.y+bounds.h;++y) {
        auto span=watchcontrols::doneRowSpan(y);
        if(span.w>0) canvas.drawFastHLine(span.x,span.y,span.w,fill);
    }
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.style().bgColor);
    watchText(canvas,"DONE",watchcontrols::doneLabelX(),watchcontrols::doneLabelY());
}

static void drawSettingsFooter() {
    const auto bounds=watchcontrols::doneBounds();
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(pressedOver(bounds.x,bounds.y,bounds.w,bounds.h)
                        ?settings.ink():settings.muted());
    watchText(canvas,"DONE",watchcontrols::doneLabelX(),watchcontrols::doneLabelY());
}

static void drawStepper(int16_t cx, const char* label, const char* value, bool selected, int16_t width) {
    drawPill(cx - width / 2, 116, width, 52, "-", selected);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    watchText(canvas, label, cx, 190);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(selected ? settings.style().accentColor : settings.ink());
    watchText(canvas, value, cx, 220);
    drawPill(cx - width / 2, 256, width, 52, "+", selected);
}

static void drawRtcSaveError() {
    if(!_rtcSaveFailed) return;
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.warning());
    watchText(canvas,"Error",233,334);
}

static void drawTimeEditor() {
    drawTitle("SET TIME");
    char hour[4], minute[4];
    snprintf(hour, sizeof(hour), "%02u", (unsigned)_editHour);
    snprintf(minute, sizeof(minute), "%02u", (unsigned)_editMinute);
    drawStepper(157,"HOUR",hour,_buttonNavigation&&_editField==0,100);
    drawStepper(309,"MINUTE",minute,_buttonNavigation&&_editField==1,100);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextColor(settings.muted());
    watchText(canvas, ":", 233, 220);
    drawRtcSaveError();
    drawFooter();
}

static void drawDateEditor() {
    drawTitle("SET DATE");
    char day[4], year[6];
    snprintf(day, sizeof(day), "%02u", (unsigned)_editDay);
    snprintf(year, sizeof(year), "%d", (int)_editYear);
    drawStepper(108,"MONTH",kMonths[_editMonth-1],_buttonNavigation&&_editField==0,86);
    drawStepper(233,"DAY",day,_buttonNavigation&&_editField==1,86);
    drawStepper(358,"YEAR",year,_buttonNavigation&&_editField==2,86);
    drawRtcSaveError();
    drawFooter();
}

static void drawFormatEditor() {
    drawTitle("TIME FORMAT");
    drawPill(78, 128, 146, 76, "12 HOUR", !settings.data().hour24, !settings.data().hour24);
    drawPill(242, 128, 146, 76, "24 HOUR", settings.data().hour24, settings.data().hour24);
    drawPill(100, 242, 266, 58, settings.data().showSeconds ? "SECONDS  ON" : "SECONDS  OFF",
             _buttonNavigation&&_editField==1, settings.data().showSeconds);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextSize(1.0f);
    canvas.setTextColor(settings.muted());
    watchText(canvas, "Show seconds on the clock", 233, 322);
    drawFooter();
}

class SettingsRowView {
public:
    explicit SettingsRowView(M5Canvas& target):_target(target) {}
    uint16_t panel() const { return settings.panel(); }
    uint16_t ink() const { return settings.ink(); }
    uint16_t muted() const { return settings.muted(); }
    uint16_t accent() const { return settings.style().accentColor; }
    bool pressed(ux::Rect bounds) const {
        return pressedOver(bounds.x,bounds.y,bounds.w,bounds.h);
    }
    void left(const char* text,int16_t x,int16_t y,uint16_t color) {
        textAt(text,x,y,color,middle_left);
    }
    void right(const char* text,int16_t x,int16_t y,uint16_t color) {
        textAt(text,x,y,color,middle_right);
    }
    void center(const char* text,int16_t x,int16_t y,uint16_t color) {
        textAt(text,x,y,color,middle_center);
    }
private:
    void textAt(const char* text,int16_t x,int16_t y,uint16_t color,uint8_t datum) {
        _target.setTextDatum(datum);
        _target.setFont(&fonts::FreeSans9pt7b);
        _target.setTextSize(1.0f);
        _target.setTextColor(color);
        watchText(_target,text,x,y);
    }
    M5Canvas& _target;
};

static void drawArrowRow(ux::Rect row, const char* label, const char* value, bool selected) {
    SettingsRowView view(canvas);
    watchsettingsrows::drawArrowRow(canvas,view,row,label,value,selected);
}

static void drawPreviewArrowRow(uint8_t index,const char* label,const char* value) {
    const auto layout=watchcontrols::previewList();
    auto row=watchcontrols::rowBounds(layout,index,_editorScroll.offset());
    if(row.y+row.h<=layout.y||row.y>=layout.y+layout.h) return;
    drawArrowRow(row,label,value,_buttonNavigation&&_editField==index);
}

static void drawAppearanceEditor() {
    drawTitle("APPEARANCE");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"SHAPE",Settings::appearanceName(settings.data().appearance));
    drawPreviewArrowRow(1,"EYES",Settings::eyeStyleName(settings.data().eyeStyle));
    canvas.clearClipRect();
    drawFooter();
}

static void drawExpressionEditor() {
    drawTitle("EXPRESSION");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, kPreviewY);
    drawPill(48, 126, 76, 96, "<", false);
    drawPill(342, 126, 76, 96, ">", false);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(settings.ink());
    watchText(canvas, Settings::expressionName(settings.data().expression), 233, 274);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(settings.muted());
    watchText(canvas, "Tap arrows; tap the bot to react", 233, 318);
    drawFooter();
}

static void drawMotionEditor() {
    drawTitle("MOTION");
    previewBot.draw();
    previewSprite.pushRotateZoomWithAA(&canvas,233,129,0,0.72f,0.72f);
    char amount[8], speed[8];
    snprintf(amount, sizeof(amount), "%u / 5", settings.data().motionAmount);
    snprintf(speed, sizeof(speed), "%u / 5", settings.data().animationSpeed);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"ACTION",Settings::animationName(settings.data().animation));
    drawPreviewArrowRow(1,"WRIST",settings.data().motion?"ON":"OFF");
    drawPreviewArrowRow(2,"INTENSITY",amount);
    drawPreviewArrowRow(3,"SPEED",speed);
    canvas.clearClipRect();
    drawFooter();
}

static void drawColorEditor() {
    drawTitle("BOT COLOR");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, 55);
    auto theme=watchcontrols::useThemeBounds();
    auto pad=watchcontrols::colorPadBounds();
    auto hue=watchcontrols::hueBarBounds();
    drawPill(theme.x,theme.y,theme.w,theme.h,"USE THEME",!settings.data().customColor);
    for(int16_t x=pad.x;x<pad.x+pad.w;x+=10) {
        int16_t tileW=(x+10<pad.x+pad.w)?10:pad.x+pad.w-x;
        int16_t sampleX=x+tileW==pad.x+pad.w?pad.x+pad.w-1:x;
        uint8_t sat=(uint8_t)((sampleX-pad.x)*100/(pad.w-1));
        for(int16_t y=pad.y;y<pad.y+pad.h;y+=6) {
            int16_t tileH=(y+6<pad.y+pad.h)?6:pad.y+pad.h-y;
            int16_t sampleY=y+tileH==pad.y+pad.h?pad.y+pad.h-1:y;
            uint8_t value=(uint8_t)((pad.y+pad.h-1-sampleY)*100/(pad.h-1));
            canvas.fillRect(x,y,tileW,tileH,
                            Settings::hsv565(settings.data().colorHue,sat,value));
        }
    }
    for(int16_t y=hue.y;y<hue.y+hue.h;++y)
        canvas.drawFastHLine(hue.x,y,hue.w,
            Settings::hsv565((uint16_t)((y-hue.y)*359/(hue.h-1)),100,100));
    int16_t sx=pad.x+settings.data().colorSat*(pad.w-1)/100;
    int16_t sy=pad.y+(100-settings.data().colorValue)*(pad.h-1)/100;
    int16_t hy=hue.y+settings.data().colorHue*(hue.h-1)/359;
    canvas.drawCircle(sx, sy, 8, settings.ink());
    ux::strokeRoundRect(canvas,hue.x-4,hy-5,hue.w+8,10,4,settings.ink());
    drawFooter();
}

static void drawDisplayEditor() {
    drawTitle("DISPLAY");
    char brightness[8];
    snprintf(brightness, sizeof(brightness), "%u / 5", settings.data().brightness);
    auto layout=watchcontrols::optionList();
    float offset=_editorScroll.offset();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawArrowRow(watchcontrols::optionRowBounds(0,offset),"BRIGHTNESS",brightness,_buttonNavigation&&_editField==0);
    drawArrowRow(watchcontrols::optionRowBounds(1,offset),"THEME",Settings::themeName(settings.data().theme),_buttonNavigation&&_editField==1);
    drawArrowRow(watchcontrols::optionRowBounds(2,offset),"INDICATOR",settings.data().indicator?"ON":"OFF",_buttonNavigation&&_editField==2);
    drawArrowRow(watchcontrols::optionRowBounds(3,offset),"BUTTON FX",settings.data().buttonFeedback?"ON":"OFF",_buttonNavigation&&_editField==3);
    drawArrowRow(watchcontrols::optionRowBounds(4,offset),"BOT TEXT",settings.data().showDescription?"SHOW":"HIDE",_buttonNavigation&&_editField==4);
    drawArrowRow(watchcontrols::optionRowBounds(5,offset),"TOP",settings.data().swapLayout?"TIME":"BOT TEXT",_buttonNavigation&&_editField==5);
    canvas.clearClipRect();
    drawFooter();
}

static void drawSoundEditor() {
    drawTitle("SOUND");
    auto layout=watchcontrols::optionList();
    float offset=_editorScroll.offset();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawArrowRow(watchcontrols::optionRowBounds(0,offset),"SOUND",settings.data().sound?"ON":"OFF",_buttonNavigation&&_editField==0);
    drawArrowRow(watchcontrols::optionRowBounds(1,offset),"STARTUP SOUND",settings.data().startupSound?"ON":"OFF",_buttonNavigation&&_editField==1);
    drawArrowRow(watchcontrols::optionRowBounds(2,offset),"BUTTON SOUND",settings.data().buttonSound?"ON":"OFF",_buttonNavigation&&_editField==2);
    drawArrowRow(watchcontrols::optionRowBounds(3,offset),"ALERT SOUND",settings.data().alertSound?"ON":"OFF",_buttonNavigation&&_editField==3);
    canvas.clearClipRect();
    drawFooter();
}

static void drawPowerEditor() {
    drawTitle("POWER SAVING");
    char dimLevel[8],from[8],until[8];
    snprintf(dimLevel,sizeof(dimLevel),"%u / 5",settings.data().dimBrightness);
    snprintf(from,sizeof(from),"%02u:00",settings.data().forcedSleepStartHour);
    snprintf(until,sizeof(until),"%02u:00",settings.data().forcedSleepEndHour);
    auto layout=watchcontrols::optionList();
    float offset=_editorScroll.offset();
    canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawArrowRow(watchcontrols::optionRowBounds(0,offset),"POWER SAVE",settings.data().powerSaveEnabled?"ON":"OFF",_buttonNavigation&&_editField==0);
    drawArrowRow(watchcontrols::optionRowBounds(1,offset),"DIM LEVEL",dimLevel,_buttonNavigation&&_editField==1);
    drawArrowRow(watchcontrols::optionRowBounds(2,offset),"DIM AFTER",watchpower::timeoutLabel(settings.data().dimTimeout),_buttonNavigation&&_editField==2);
    drawArrowRow(watchcontrols::optionRowBounds(3,offset),"AUTO OFF",watchpower::timeoutLabel(settings.data().screenOffTimeout),_buttonNavigation&&_editField==3);
    drawArrowRow(watchcontrols::optionRowBounds(4,offset),"WAKE",settings.data().wakeMode==Settings::WAKE_KEYS_ONLY?"KEYS ONLY":"TOUCH + KEYS",_buttonNavigation&&_editField==4);
    drawArrowRow(watchcontrols::optionRowBounds(5,offset),"CHARGE AWAKE",settings.data().keepAwakeWhileCharging?"ON":"OFF",_buttonNavigation&&_editField==5);
    drawArrowRow(watchcontrols::optionRowBounds(6,offset),"FORCED OFF",settings.data().forcedSleepEnabled?"ON":"OFF",_buttonNavigation&&_editField==6);
    drawArrowRow(watchcontrols::optionRowBounds(7,offset),"FROM",from,_buttonNavigation&&_editField==7);
    drawArrowRow(watchcontrols::optionRowBounds(8,offset),"UNTIL",until,_buttonNavigation&&_editField==8);
    canvas.clearClipRect();
    drawFooter();
}

static void drawNameEditor() {
    drawTitle("BOT NAME");
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(settings.ink());
    watchEllipsizedText(canvas,_nameEditor.text(),233,96,310);
    static const ux::NameKeyboardLabels zhKeys={"删除","空格","Aa","确定"};
    ux::drawNameKeyboard(canvas,_nameEditor,watchcontrols::nameKeyboardBounds(),
                         settings.panel(), settings.ink(), watchKeyboardFont(settings.data().language),
                         _namePressedKey,settings.data().language?&zhKeys:nullptr);
    drawFooter();
}

static void drawLanguageEditor() {
    drawTitle("LANGUAGE");
    drawPill(72, 160, 152, 70, "English", settings.data().language == 0);
    drawPill(242, 160, 152, 70, "中文", settings.data().language == 1);
    drawFooter();
}

static void drawLayoutEditor() {
    drawTitle("WATCH LAYOUT");
    drawArrowRow({58,148,350,44},"BOT TEXT",settings.data().showDescription?"SHOW":"HIDE",_buttonNavigation&&_editField==0);
    drawArrowRow({58,228,350,44},"TOP",settings.data().swapLayout?"TIME":"BOT TEXT",_buttonNavigation&&_editField==1);
    drawFooter();
}

static void drawCombinationEditor() {
    drawTitle("COMBINATIONS");
    previewBot.draw();
    previewSprite.pushSprite(&canvas, kPreviewX, 62);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"STATE",botux::BotUx::moodName((botux::BotUx::Mood)_previewMood));
    drawPreviewArrowRow(1,"FACE",botux::BotUx::expressionName((botux::BotUx::Expression)_previewExpression));
    drawPreviewArrowRow(2,"ACTION",botux::BotUx::animationName((botux::BotUx::Animation)_previewAnimation));
    canvas.clearClipRect();
    drawFooter();
}

static void drawGazeEditor() {
    drawTitle("GAZE"); previewBot.draw();
    previewSprite.pushSprite(&canvas,kPreviewX,62);
    auto layout=watchcontrols::previewList(); canvas.setClipRect(layout.x,layout.y,layout.w,layout.h);
    drawPreviewArrowRow(0,"DIRECTION",botux::BotUx::gazeDirectionName((botux::BotUx::GazeDirection)settings.data().gaze,settings.data().language?botux::BotUx::Language::Chinese:botux::BotUx::Language::English));
    canvas.clearClipRect();
    drawFooter();
}

static void drawEditor() {
    switch (_editor) {
        case Editor::Time:       drawTimeEditor(); break;
        case Editor::Date:       drawDateEditor(); break;
        case Editor::Format:     drawFormatEditor(); break;
        case Editor::Expression: drawExpressionEditor(); break;
        case Editor::Appearance: drawAppearanceEditor(); break;
        case Editor::Motion:     drawMotionEditor(); break;
        case Editor::Color:      drawColorEditor(); break;
        case Editor::Display:    drawDisplayEditor(); break;
        case Editor::Sound:      drawSoundEditor(); break;
        case Editor::Power:      drawPowerEditor(); break;
        case Editor::Name: drawNameEditor(); break;
        case Editor::Preview: drawCombinationEditor(); break;
        case Editor::Layout: drawLayoutEditor(); break;
        case Editor::Language: drawLanguageEditor(); break;
        case Editor::Gaze: drawGazeEditor(); break;
        default: break;
    }
}

static bool previewIsAnimated() {
    return _screen == Screen::Editor
        && (_editor == Editor::Gaze || _editor == Editor::Preview || _editor == Editor::Expression || _editor == Editor::Appearance
            || _editor == Editor::Motion || _editor == Editor::Color);
}

static void recordFrame(uint32_t now, uint32_t updateUs, uint32_t drawUs, uint32_t pushUs) {
    uint8_t screen = (uint8_t)_screen;
    if (_telemetry.screen != screen) {
        _telemetry = {};
        _telemetry.screen = screen;
    }
    if (_telemetry.windowStartMs == 0) _telemetry.windowStartMs = now;
    ++_telemetry.frames;
    _telemetry.updateUs += updateUs;
    _telemetry.drawUs += drawUs;
    _telemetry.pushUs += pushUs;
    uint32_t frameUs = updateUs + drawUs + pushUs;
    if (frameUs > _telemetry.maxFrameUs) _telemetry.maxFrameUs = frameUs;
    uint32_t elapsed = now - _telemetry.windowStartMs;
    if (elapsed < 5000 || _telemetry.frames == 0) return;

    Serial.printf("PERF screen=%u fps=%.1f update=%luus draw=%luus push=%luus max=%luus heap=%u largest=%u psram=%u sound=%u audio_state=%u audio_suspended=%u sound_fail=%lu\n",
                  (unsigned)_screen, (double)_telemetry.frames * 1000.0 / elapsed,
                  (unsigned long)(_telemetry.updateUs / _telemetry.frames),
                  (unsigned long)(_telemetry.drawUs / _telemetry.frames),
                  (unsigned long)(_telemetry.pushUs / _telemetry.frames),
                  (unsigned long)_telemetry.maxFrameUs,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  (unsigned)ESP.getFreePsram(),_soundOutput.ready(),
                  (unsigned)_soundOutput.powerState(),_soundOutput.suspended(),
                  (unsigned long)_soundOutput.failures());
    _telemetry = {};
    _telemetry.windowStartMs = now;
    _telemetry.screen = screen;
}

static void drawTouchCalibration() {
    canvas.fillSprite(settings.style().bgColor);
    canvas.setTextDatum(middle_center);
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.setTextColor(settings.ink());
    char progress[24];
    if(_touchCalibrationMode==TouchCalibrationMode::Passed) {
        canvas.setTextColor(TFT_GREEN);
        snprintf(progress,sizeof(progress),"5 / 5  VERIFY PASS");
        watchText(canvas,progress,kW/2,kH/2,false);
        return;
    }
    if(_touchCalibrationMode==TouchCalibrationMode::Failed) {
        canvas.setTextColor(TFT_RED);
        snprintf(progress,sizeof(progress),"CALIBRATION FAIL");
        watchText(canvas,progress,kW/2,kH/2,false);
        return;
    }
    uint8_t count=touchCalibrationCount();
    if(_touchCalibrationIndex>=count) {
        if(_touchCalibrationMode==TouchCalibrationMode::Repeat) {
            snprintf(progress,sizeof(progress),"9/9 COMPLETE");
        } else if(_touchCalibrationMode==TouchCalibrationMode::Direction) {
            snprintf(progress,sizeof(progress),"6/6 COMPLETE");
        } else {
            snprintf(progress,sizeof(progress),"5 / 5  COMPLETE");
        }
        watchText(canvas,progress,kW/2,kH/2,false);
        return;
    }
    if(_touchCalibrationMode==TouchCalibrationMode::Direction
       &&_touchCalibrationDirectionGate) {
        watchText(canvas,kTouchDirectionTurned[_touchCalibrationIndex]
                  ?"ROTATE 180":"NORMAL",kW/2,190,false);
        watchText(canvas,"PRESS A/B",kW/2,276,false);
        return;
    }
    if(_touchCalibrationMode==TouchCalibrationMode::Direction
       &&_touchCalibrationAwaitRelease) return;
    if(_touchCalibrationMode==TouchCalibrationMode::Direction) {
        snprintf(progress,sizeof(progress),"%u/6",_touchCalibrationIndex+1);
        watchText(canvas,progress,kW/2,44,false);
        int16_t x=kTouchDirectionTargets[_touchCalibrationIndex][0];
        int16_t y=kTouchDirectionTargets[_touchCalibrationIndex][1];
        uint16_t accent=_touchCalibrationRetry?TFT_RED:TFT_CYAN;
        canvas.drawCircle(x,y,28,accent);
        canvas.drawCircle(x,y,27,accent);
        canvas.drawFastHLine(x-20,y,41,accent);
        canvas.drawFastVLine(x,y-20,41,accent);
        canvas.fillCircle(x,y,4,accent);
        return;
    }
    const char* label=_touchCalibrationMode==TouchCalibrationMode::Train?"TRAIN"
        :_touchCalibrationMode==TouchCalibrationMode::Verify?"VERIFY"
        :_touchCalibrationMode==TouchCalibrationMode::Repeat?"REPEAT":"TOUCH TEST";
    snprintf(progress,sizeof(progress),"%s  %u / %u",label,_touchCalibrationIndex+1,count);
    watchText(canvas,progress,kW/2,44,false);
    const auto* targets=touchCalibrationTargets();
    int16_t x=targets[_touchCalibrationIndex][0];
    int16_t y=targets[_touchCalibrationIndex][1];
    if(_touchCalibrationRetry) {
        canvas.setTextColor(TFT_RED);
        watchText(canvas,"HOLD STEADY - RETRY",kW/2,y<180?350:76,false);
    }
    uint16_t accent=_touchCalibrationRetry?TFT_RED
        :_touchCalibrationMode==TouchCalibrationMode::Verify?TFT_YELLOW:TFT_CYAN;
    canvas.drawCircle(x,y,28,accent);
    canvas.drawCircle(x,y,27,accent);
    canvas.drawFastHLine(x-20,y,41,accent);
    canvas.drawFastVLine(x,y-20,41,accent);
    canvas.fillCircle(x,y,4,accent);
}

static uint8_t visibleButtonMask() {
    if (!settings.data().buttonFeedback) return 0;
    return _diagnosticButtons ? _diagnosticButtonMask : _buttonFeedback.held();
}

static size_t buttonMaskPixels(watchbuttons::Button button) {
    auto bounds = watchbuttons::dirtyBounds(button);
    return (size_t)bounds.w * bounds.h;
}

static uint8_t* buttonMask(watchbuttons::Button button, uint8_t step) {
    size_t offset = button == watchbuttons::A ? 0
                  : button == watchbuttons::B
                      ? kButtonMaskBytes * kButtonSideMaskPixels
                      : kButtonMaskBytes * kButtonSideMaskPixels * 2;
    return _buttonFeedbackMasks + offset + buttonMaskPixels(button) * step;
}

static uint32_t buildButtonFeedbackMasks() {
    uint32_t started = millis();
    static_assert(kButtonMasksTotal == watchbuttonmasks::DecodedSize,
                  "generated button masks must match runtime layout");
    return watchbuttonmasks::decode(_buttonFeedbackMasks, kButtonMasksTotal)
        ? millis() - started : UINT32_MAX;
}

static void blendButtonBlob(lgfx::rgb565_t* pixels, int16_t width,
                            int16_t bufferX, int16_t bufferY,
                            const watchbuttons::Bounds& bounds,
                            watchbuttons::Button button) {
    uint8_t step = _diagnosticButtons ? watchbuttons::Feedback::ExpandSteps
                                      : _buttonFeedback.expandStep(button);
    const auto full = watchbuttons::dirtyBounds(button);
    const uint8_t* mask = buttonMask(button, step);
    uint16_t color = watchbuttons::restingBlob(button).color;
    for (int16_t y = 0; y < bounds.h; ++y) {
        for (int16_t x = 0; x < bounds.w; ++x) {
            size_t maskIndex = (size_t)(bounds.y + y - full.y) * full.w
                             + (bounds.x + x - full.x);
            uint8_t coverage = mask[maskIndex];
            if (!coverage) continue;
            size_t index = (size_t)(bounds.y + y - bufferY) * width
                         + (bounds.x + x - bufferX);
            // Typed M5GFX pixels keep read, blend and push in native RGB565;
            // the uint16_t overload instead uses transport byte order.
            pixels[index] = coverage == 255 ? color
                : ux::blend565(pixels[index].raw, color, coverage);
        }
    }
}

static void compositeButtonFeedbackIntoCanvas(M5Canvas& target) {
    uint8_t held = visibleButtonMask();
    const watchbuttons::Button buttons[] = {
        watchbuttons::A, watchbuttons::B, watchbuttons::Power
    };
    for (auto button : buttons) {
        if (!(held & button)) continue;
        auto bounds = watchbuttons::dirtyBounds(button);
        target.readRect(bounds.x, bounds.y, bounds.w, bounds.h,
                        _buttonFeedbackScratch);
        blendButtonBlob(_buttonFeedbackScratch, bounds.w, bounds.x, bounds.y,
                        bounds, button);
        target.pushImage(bounds.x, bounds.y, bounds.w, bounds.h,
                         _buttonFeedbackScratch);
    }
}

static void renderButtonFeedbackFromCanvas(M5Canvas& base, bool fullBaseWasPushed) {
    uint8_t held = visibleButtonMask();
    uint8_t dirty = fullBaseWasPushed ? held : (uint8_t)(_buttonFeedbackDrawnMask | held);
    if (!_buttonFeedbackDirty && !fullBaseWasPushed && !held) return;
    uint32_t started = micros(), pixels = 0;
    const watchbuttons::Button buttons[] = {
        watchbuttons::A, watchbuttons::B, watchbuttons::Power
    };
    for (auto button : buttons) {
        if (!(dirty & button)) continue;
        auto bounds = watchbuttons::dirtyBounds(button);
        base.readRect(bounds.x, bounds.y, bounds.w, bounds.h,
                      _buttonFeedbackScratch);
        if (held & button)
            blendButtonBlob(_buttonFeedbackScratch, bounds.w, bounds.x, bounds.y,
                            bounds, button);
        M5.Display.pushImage(bounds.x, bounds.y, bounds.w, bounds.h,
                             _buttonFeedbackScratch);
        pixels += (uint32_t)bounds.w * bounds.h;
    }
    M5.Display.waitDisplay();
    _buttonFeedbackDrawnMask = held;
    _buttonFeedbackDirty = false;
    _buttonFeedbackRenderUs = micros() - started;
    if (_buttonFeedbackRenderUs > _buttonFeedbackRenderMaxUs)
        _buttonFeedbackRenderMaxUs = _buttonFeedbackRenderUs;
    _buttonFeedbackPixels = pixels;
}

static uint32_t pushCanvasRegionWithButtonFeedback(
        M5Canvas& base, const watchbuttons::Bounds& area, uint8_t held) {
    int16_t tileHeight = (int16_t)(kButtonScratchPixels / area.w);
    uint32_t pixels = 0;
    const watchbuttons::Button buttons[] = {
        watchbuttons::A, watchbuttons::B, watchbuttons::Power
    };
    for (int16_t y = area.y; y < area.y + area.h; y += tileHeight) {
        int16_t height = y + tileHeight < area.y + area.h
                       ? tileHeight : area.y + area.h - y;
        watchbuttons::Bounds tile{area.x, y, area.w, height};
        base.readRect(tile.x, tile.y, tile.w, tile.h, _buttonFeedbackScratch);
        for (auto button : buttons) {
            if (!(held & button)) continue;
            auto clipped = watchfeedbackpatch::intersection(
                tile, watchbuttons::dirtyBounds(button));
            if (clipped.w && clipped.h)
                blendButtonBlob(_buttonFeedbackScratch, tile.w, tile.x, tile.y,
                                clipped, button);
        }
        // pushImage is the synchronous, non-DMA API; the same fixed scratch
        // storage is therefore safe to refill for the next tile immediately.
        M5.Display.pushImage(tile.x, tile.y, tile.w, tile.h,
                             _buttonFeedbackScratch);
        pixels += (uint32_t)tile.w * tile.h;
    }
    return pixels;
}

static void pushFaceWithButtonFeedback(uint32_t now) {
    uint8_t held = visibleButtonMask();
    uint8_t dirty = (uint8_t)(_buttonFeedbackDrawnMask | held);
    uint32_t started = micros(), pixels = 0;

    // Keep a clean face base in PSRAM while feedback is visible. Animation
    // steps reuse it; only the bot and the infrequently-changing HUD are
    // refreshed, and liquid is always blended in the separate scratch buffer.
    bool fullFrame = _faceNeedsClear;
    bool submitHud = _buttonFeedbackHudMs && now - _buttonFeedbackHudMs >= 200;
    bool refreshHud = !_buttonFeedbackFaceBaseValid || fullFrame || submitHud;
    if (!_buttonFeedbackFaceBaseValid || fullFrame) {
        canvas.fillSprite(settings.style().bgColor);
        _buttonFeedbackFaceBaseValid = true;
    }
    // Edge patches never enter the Bot rectangle. Keep its current pixels in
    // the sprite and panel; only a full-frame transition needs the Bot copied
    // into the canonical canvas.
    if (fullFrame) botSprite.pushSprite(&canvas, kBotX, kBotY);
    if (refreshHud) {
        face.invalidate();
        face.draw(&canvas, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  statusPanelProgress(now));
        _buttonFeedbackHudMs = now;
    }

    if (fullFrame) {
        // A screen transition needs every old settings pixel replaced, but the
        // finished frame is transferred once; there is no visible blank pass.
        pixels = pushCanvasRegionWithButtonFeedback(
            canvas, watchbuttons::Bounds{0, 0, kW, kH}, held);
        _faceNeedsClear = false;
    } else {
        if (submitHud) {
            pixels += pushCanvasRegionWithButtonFeedback(
                canvas, watchbuttons::Bounds{0, 0, kW, 90}, held);
            pixels += pushCanvasRegionWithButtonFeedback(
                canvas, watchbuttons::Bounds{0, 376, kW, 90}, held);
        }
        botSprite.pushSprite(&M5.Display, kBotX, kBotY);
        pixels += (uint32_t)kBotSize * kBotSize;
    }

    // Do not retransmit Bot pixels inside broad liquid dirty boxes. When both
    // HUD bands were already submitted with their liquid blended in, keep the
    // remaining patches in the middle band as well. The subtraction produces
    // a few rectangles rather than a panel transaction per scanline.
    const watchbuttons::Button buttons[] = {
        watchbuttons::A, watchbuttons::B, watchbuttons::Power
    };
    const watchbuttons::Bounds exclusions[] = {
        {kBotX, kBotY, kBotSize, kBotSize},
        {0, 0, kW, 90},
        {0, 376, kW, 90},
    };
    for (auto button : buttons) {
        if (fullFrame) break;
        if (!(dirty & button)) continue;
        auto bounds = watchbuttons::dirtyBounds(button);
        auto regions = watchfeedbackpatch::visibleRegions(
            bounds, exclusions, submitHud ? 3 : 1);
        for (uint8_t i = 0; i < regions.count; ++i) {
            auto piece = regions.items[i];
            canvas.readRect(piece.x, piece.y, piece.w, piece.h,
                            _buttonFeedbackScratch);
            if (held & button)
                blendButtonBlob(_buttonFeedbackScratch, piece.w,
                                piece.x, piece.y, piece, button);
            M5.Display.pushImage(piece.x, piece.y, piece.w, piece.h,
                                 _buttonFeedbackScratch);
            pixels += (uint32_t)piece.w * piece.h;
        }
    }
    M5.Display.waitDisplay();
    _buttonFeedbackDrawnMask = held;
    _buttonFeedbackDirty = false;
    _buttonFeedbackRenderUs = micros() - started;
    if (_buttonFeedbackRenderUs > _buttonFeedbackRenderMaxUs)
        _buttonFeedbackRenderMaxUs = _buttonFeedbackRenderUs;
    _buttonFeedbackPixels = pixels;
}

static void render(uint32_t now) {
    if(_touchCalibration) {
        if(!_uiDirty) return;
        drawTouchCalibration();
        canvas.pushSprite(0,0);
        M5.Display.waitDisplay();
        _uiDirty=false;
        _listBandOnly=false;
        return;
    }
    uint32_t t0 = micros();
    face.setBattery(visibleBattery());
    face.setCharging(_charging);
    face.setHour24(settings.data().hour24);
    face.setShowSeconds(settings.data().showSeconds);
    auto mood = faceMood(now);
    if (mood != _drawMood) {
        face.bot().setMood(mood, 700);
        _drawMood = mood;
    }
    auto selectedExpression = (botux::BotUx::Expression)settings.data().expression;
    bool gazeActive=_screen==Screen::Face&&directedGazeActive(now);
    auto presentation=watchcompanion::presentation(gazeActive,_manualPreset,(uint8_t)_manualCombo.mood,
        (uint8_t)_manualCombo.expression,(uint8_t)_manualCombo.animation,
        (uint8_t)selectedExpression,settings.data().animation,_dozing||_battery<=kLowBattery);
    face.bot().setExpression((botux::BotUx::Expression)presentation.expression);
    face.bot().setAnimation((botux::BotUx::Animation)presentation.animation);
    face.bot().setTalking(presentation.talking);
    face.update(now);
    // The HUD keeps the real percentage. Explicit previews temporarily suppress
    // BotUx's <=10% sleepy override so every selected mood remains visible.
    face.bot().setBattery(!_dozing&&_screen==Screen::Face&&(_manualPreset||gazeActive)?100:_battery);
    face.bot().setBatteryVisible(false);
    face.bot().update(now);

    if (previewIsAnimated()) {
        if (_editor == Editor::Preview) {
            previewBot.setMood((botux::BotUx::Mood)_previewMood);
            previewBot.setTalking(_previewMood == (uint8_t)botux::BotUx::Mood::Speaking);
            previewBot.setExpression((botux::BotUx::Expression)_previewExpression);
            previewBot.setAnimation((botux::BotUx::Animation)_previewAnimation);
        } else {
            previewBot.setTalking(false);
            previewBot.setMood(botux::BotUx::Mood::Listening);
            previewBot.setExpression((botux::BotUx::Expression)settings.data().expression);
            previewBot.setAnimation((botux::BotUx::Animation)settings.data().animation);
        }
        previewBot.update(now);
    }
    uint32_t t1 = micros();

    if (_screen == Screen::Face) {
        if (_buttonFeedbackDirty || visibleButtonMask() || _buttonFeedbackDrawnMask) {
            face.bot().draw();
            uint32_t t2 = micros();
            bool compose = _buttonFeedbackDirty || _faceNeedsClear
                || !_buttonFeedbackHudMs || now - _buttonFeedbackHudMs >= 200;
            if (compose) pushFaceWithButtonFeedback(now);
            else {
                // The bot cannot intersect any liquid pixel (exhaustively
                // checked by the geometry test), so its normal animation can
                // update without rebuilding or erasing the settled overlay.
                botSprite.pushSprite(&M5.Display, kBotX, kBotY);
                M5.Display.waitDisplay();
            }
            uint32_t t3 = micros();
            recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
            return;
        }
        if (_faceNeedsClear) {
            M5.Display.fillScreen(settings.style().bgColor);
            _faceNeedsClear = false;
        }
        face.bot().draw();
        uint32_t t2 = micros();
        botSprite.pushSprite(&M5.Display, kBotX, kBotY);
        face.draw(&M5.Display, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  statusPanelProgress(now));
        _buttonFeedbackHudMs = now;
        _buttonFeedbackFaceBaseValid = false;
        M5.Display.waitDisplay();
        uint32_t t3 = micros();
        recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
        return;
    }

    if (!_uiDirty && !previewIsAnimated() && !_buttonFeedbackDirty) return;
    bool baseWasPushed = _uiDirty || previewIsAnimated();
    bool bandOnly = _listBandOnly && (_screen == Screen::Settings
        || _screen == Screen::TimeSettings || _screen == Screen::Personalize);
    const auto mainLayout=watchcontrols::mainList();
    const auto carouselBand=watchcontrols::settingsCarouselMotionBand();
    watchbuttons::Bounds listArea=_screen==Screen::Settings
        ?watchbuttons::Bounds{(int16_t)carouselBand.x,(int16_t)carouselBand.y,
                             (int16_t)carouselBand.w,(int16_t)carouselBand.h}
        :watchbuttons::Bounds{0,mainLayout.y,kW,mainLayout.h};
    if (baseWasPushed) {
        const uint16_t background=_screen==Screen::Settings?0:settings.style().bgColor;
        if(bandOnly) canvas.fillRect(listArea.x,listArea.y,listArea.w,listArea.h,background);
        else canvas.fillSprite(background);
        if (_screen == Screen::Settings) {
            drawSettingsList(!bandOnly);
        } else if (_screen == Screen::TimeSettings) {
            drawTimeSettingsList(!bandOnly);
        } else if (_screen == Screen::Personalize) {
            drawPersonalizeList(!bandOnly);
        } else {
            drawEditor();
        }
        drawPointerFeedback();
    }
    uint32_t t2 = micros();
    uint8_t held = visibleButtonMask();
    bool compositeBase = baseWasPushed
        && (held || _buttonFeedbackDrawnMask || _buttonFeedbackDirty);
    if (baseWasPushed) {
        if (compositeBase) {
            watchbuttons::Bounds area = bandOnly
                ? listArea
                : watchbuttons::Bounds{0, 0, kW, kH};
            uint32_t started = micros();
            _buttonFeedbackPixels = pushCanvasRegionWithButtonFeedback(canvas, area, held);
            _buttonFeedbackRenderUs = micros() - started;
            if (_buttonFeedbackRenderUs > _buttonFeedbackRenderMaxUs)
                _buttonFeedbackRenderMaxUs = _buttonFeedbackRenderUs;
            if (!bandOnly) {
                _buttonFeedbackDrawnMask = held;
                _buttonFeedbackDirty = false;
            }
        } else if(bandOnly) M5.Display.pushImage(listArea.x,listArea.y,listArea.w,listArea.h,
            (uint16_t*)canvas.getBuffer()+kW*listArea.y+listArea.x);
        else canvas.pushSprite(0, 0);
    }
    if (!baseWasPushed || bandOnly)
        renderButtonFeedbackFromCanvas(canvas, false);
    M5.Display.waitDisplay();
    uint32_t t3 = micros();
    _uiDirty = false;
    _listBandOnly = false;
    recordFrame(now, t1 - t0, t2 - t1, t3 - t2);
}

static void emitFrameCapture() {
    // A capture is an independent full-frame render. Do not inherit a partial
    // update clip from the preceding animation frame or diagnostic command.
    canvas.clearClipRect();
    if(_touchCalibration) {
        drawTouchCalibration();
    } else if (_screen == Screen::Face) {
        canvas.fillSprite(settings.style().bgColor);
        botSprite.pushSprite(&canvas, kBotX, kBotY);
        face.invalidate();
        face.draw(&canvas, settings.style().bgColor, settings.ink(), settings.muted(),
                  settings.style().accentColor, settings.panel(), settings.warning(),
                  statusPanelProgress(millis()));
    } else {
        canvas.fillSprite(_screen==Screen::Settings?0:settings.style().bgColor);
        if (_screen == Screen::Settings) drawSettingsList(true);
        else if (_screen == Screen::TimeSettings) drawTimeSettingsList(true);
        else if (_screen == Screen::Personalize) drawPersonalizeList(true);
        else drawEditor();
        drawPointerFeedback();
    }
    canvas.clearClipRect();
    if(!_touchCalibration) compositeButtonFeedbackIntoCanvas(canvas);

    const uint8_t* pixels = (const uint8_t*)canvas.getBuffer();
    size_t bytes = canvas.bufferLength();
    Serial.printf("FRAME %d %d RGB565BE %u\n", kW, kH, (unsigned)bytes);
    for (size_t offset = 0; offset < bytes; offset += 2048) {
        size_t chunk = bytes - offset;
        if (chunk > 2048) chunk = 2048;
        Serial.write(pixels + offset, chunk);
        yield();
    }
    Serial.print("\nFRAME_END\n");
    Serial.flush();
    _buttonFeedbackFaceBaseValid = false;
    // Frame capture temporarily composites diagnostics into the shared canvas.
    // Rebuild the normal page before it is reused as an overlay base.
    if (_screen != Screen::Face && !_touchCalibration) {
        _uiDirty = true;
        _listBandOnly = false;
    }
}

static void selectDiagnosticPage(uint8_t page) {
    _diagnosticContact=false; _diagnosticDown=false; _contact.sample(false,0,0);
    resetUiPointer();
    _diagnosticScroll = page == 19;
    static bool override = false;
    static Settings::Data prior;
    if (override) { settings.data() = prior; applySettings(); override = false; }
    if (page == 16 || page == 17) {
        prior = settings.data(); override = true;
        settings.data().language = page == 16 ? 1 : 0;
        settings.data().swapLayout = page == 17;
        settings.data().showDescription = true;
        applySettings();
    }
    _originalSettings = settings.data();
    _editor = Editor::None;
    _editorParent = Screen::Personalize;
    _personalParent = Screen::Settings;
    if (page == 0 || page == 10 || page == 16 || page == 17) {
        _screen = Screen::Face;
        _faceNeedsClear = true;
        face.invalidate();
        if (page == 10) {
            showStatusPanel(millis(), 6000);
            _statusPanelStartMs -= 300;
        }
    } else if (page == 1 || page == 8 || page == 18 || page == 19) {
        _screen = Screen::Settings;
        _settingsCarousel.configure(kMenuCount,watchcontrols::settingsCarousel().step);
        _settingsCarousel.select(page==8?kMenuCount-1:0);
        if(page==18) _settingsCarousel.setOffset(watchcontrols::settingsCarousel().step*0.5f);
        _menu=(MenuItem)_settingsCarousel.selected();
    } else if(page==23) {
        _screen=Screen::TimeSettings;
        _timeList.configure(kTimeCount,kVisibleRows);
        _timeList.select(0); _timeItem=TimeItem::Time;
        _timeScroll.setBounds(kTimeCount*kMenuStep,watchcontrols::mainList().h);
        _timeScroll.setOffset(0);
    } else if (page == 2 || page == 9) {
        _screen = Screen::Personalize;
        _personalList.configure(kPersonalCount, kVisibleRows);
        _personalList.select(page == 9 ? kPersonalCount-1 : 0);
        _personal = (PersonalItem)_personalList.selected();
        _personalScroll.setBounds(kPersonalCount*kMenuStep,watchcontrols::mainList().h);
        _personalScroll.setOffset(page == 9 ? 9999 : 0);
    } else {
        _screen = Screen::Editor;
        _editor = page == 3 ? Editor::Expression
                : page == 4 ? Editor::Appearance
                : page == 5 ? Editor::Motion
                : page == 6 ? Editor::Color
                : page == 7 ? Editor::Display
                : page == 11 ? Editor::Format
                : page == 12 ? Editor::Name
                : (page == 13 || page == 20 || page == 22) ? Editor::Preview
                : page == 14 ? Editor::Layout
                : page == 15 ? Editor::Language
                : page == 21 ? Editor::Gaze
                : page == 24 ? Editor::Sound
                : page == 25 ? Editor::Power : Editor::Time;
        if (page == 12) _nameEditor.begin(settings.data().botName);
        if (page == 22) { _previewMood=(uint8_t)botux::BotUx::Mood::Thinking; _previewExpression=_previewAnimation=0; }
        if (page == 20) {
            _previewMood = (uint8_t)botux::BotUx::Mood::Happy;
            _previewExpression = (uint8_t)botux::BotUx::Expression::Joy;
            _previewAnimation = (uint8_t)botux::BotUx::Animation::Wave;
        }
        _editField = 0;
        if(watchcontrols::editorUsesScrollList(_editor)) {
            auto layout=watchcontrols::editorList(_editor);
            _editorScroll.setBounds(watchcontrols::editorRowCount(_editor)*layout.step,layout.h);
            _editorScroll.setOffset(page==25?9999:0);
        }
    }
    _uiDirty = true;
}

static uint32_t _diagnosticSeq=0;
static void printUiState() {
    auto& ioe1=M5.getIOExpander(0);
    bool audioPower=false,audioPa=false;
    bool audioIoValid=ioe1.getInputLevel(kAudioPowerIo,&audioPower)
        &&ioe1.getInputLevel(kAudioAmplifierIo,&audioPa);
    Serial.printf("UI seq=%lu screen=%u editor=%u pressed=%d scrolling=%u offset=%.1f sound=%u startup_sound=%u button_sound=%u alert_sound=%u audio_desired=%u audio_bound=%u audio_state=%u audio_ready=%u audio_suspended=%u audio_failed=%u audio_io_valid=%u audio_power=%u audio_pa=%u language=%u gaze=%u indicator=%u button_fx=%u status=%u charging=%u manual=%u mood=%u effective=%u expression=%u effective_expr=%u animation=%u keys_held=%u pwr_valid=%u pwr_held=%u keys_diag=%u keys_px=%lu keys_us=%lu keys_max_us=%lu power_save=%u dim_level=%u dim_timeout=%u off_timeout=%u wake_mode=%u charge_awake=%u force_sleep=%u force_start=%u force_end=%u screen_power=%u\n",
        (unsigned long)_diagnosticSeq,(unsigned)_screen,(unsigned)_editor,exactPressedTarget(),_pointer.scrolling(),
        _screen==Screen::Settings?_settingsCarousel.offset():_screen==Screen::TimeSettings?_timeScroll.offset():_screen==Screen::Personalize?_personalScroll.offset():_screen==Screen::Editor?_editorScroll.offset():0,settings.data().sound,settings.data().startupSound,settings.data().buttonSound,settings.data().alertSound,soundDesired(),_soundBound,(unsigned)_soundOutput.powerState(),_soundOutput.ready(),_soundOutput.suspended(),_soundOutput.failed(),audioIoValid,audioPower,audioPa,settings.data().language,settings.data().gaze,settings.data().indicator,settings.data().buttonFeedback,_statusPanelUntilMs!=0,_charging,
        _manualPreset,(unsigned)face.bot().mood(),(unsigned)face.bot().effectiveMood(),(unsigned)face.bot().expression(),(unsigned)face.bot().effectiveExpression(),(unsigned)face.bot().animation(),
        (unsigned)_buttonFeedback.held(),_powerButtonValid,_powerButtonPressed,_diagnosticButtons,
        (unsigned long)_buttonFeedbackPixels,(unsigned long)_buttonFeedbackRenderUs,
        (unsigned long)_buttonFeedbackRenderMaxUs,settings.data().powerSaveEnabled,
        settings.data().dimBrightness,settings.data().dimTimeout,
        settings.data().screenOffTimeout,settings.data().wakeMode,
        settings.data().keepAwakeWhileCharging,settings.data().forcedSleepEnabled,
        settings.data().forcedSleepStartHour,settings.data().forcedSleepEndHour,
        (unsigned)_screenPowerState);
    // Drain this diagnostic reply now, rather than waiting for later telemetry.
    Serial.flush();
}
static void handleSerialCommands() {
    static char line[48]; static uint8_t length=0;
    while(Serial.available()) {
        char c=(char)Serial.read();
        if(c!='\n' && c!='\r') { if(length+1<sizeof(line)) line[length++]=c; continue; }
        if(!length) continue;
        line[length]=0; length=0;
        char* seq=strchr(line,'@'); _diagnosticSeq=seq?strtoul(seq+1,nullptr,10):0;
        if(seq) { while(seq>line && seq[-1]==' ') --seq; *seq=0; }
        int x,y,down,mask,pct;
        if(sscanf(line,"contact %d %d %d",&down,&x,&y)==3) {
            _diagnosticContact=true; _diagnosticDown=down!=0;
            _diagnosticX=x; _diagnosticY=y; _contactReplyPending=true;
        } else if(!strcmp(line,"physical")) { _diagnosticContact=false; printUiState(); }
        else if(!strcmp(line,"keys off")) {
            _diagnosticButtons=false; _diagnosticButtonMask=0;
            invalidateButtonFeedback(); printUiState();
        }
        else if(sscanf(line,"keys %d",&mask)==1 && mask>=0 && mask<=7) {
            _diagnosticButtons=true; _diagnosticButtonMask=(uint8_t)mask;
            invalidateButtonFeedback(); printUiState();
        }
        else if(!strcmp(line,"battery off")) {
            _diagnosticBattery=false;
            _faceNeedsClear=true; face.invalidate(); showStatusPanel(millis()); printUiState();
        }
        else if(sscanf(line,"battery %d",&pct)==1 && pct>=0 && pct<=100) {
            _diagnosticBattery=true; _diagnosticBatteryPct=(uint8_t)pct;
            _faceNeedsClear=true; face.invalidate(); showStatusPanel(millis()); printUiState();
        }
        else if(!strcmp(line,"cal on")) { _diagnosticContact=false; startTouchCalibration(); Serial.println("CAL armed 1/5"); Serial.flush(); }
        else if(!strcmp(line,"cal start")) { _diagnosticContact=false; startTouchCalibration(TouchCalibrationMode::Train); Serial.println("CAL training 1/5"); Serial.flush(); }
        else if(!strcmp(line,"cal repeat")) { _diagnosticContact=false; startTouchCalibration(TouchCalibrationMode::Repeat); Serial.println("CAL repeat 1/9"); Serial.flush(); }
        else if(!strcmp(line,"cal direction")) { _diagnosticContact=false; startTouchCalibration(TouchCalibrationMode::Direction); Serial.println("CAL direction normal gate 1/6"); Serial.flush(); }
        else if(!strcmp(line,"cal off")) { stopTouchCalibration(); Serial.println("CAL disarmed"); Serial.flush(); }
        else if(!strcmp(line,"cal dump")) dumpTouchCalibration();
        else if(!strcmp(line,"cal map")) dumpTouchMapping();
        else if(!strcmp(line,"cal profile")) printTouchAffineProfile();
        else if(!strcmp(line,"cal save")) { Serial.printf("CAL save=%u\n",saveTouchAffine()); Serial.flush(); }
        else if(!strcmp(line,"cal reset")) {
            if(_touchCalibration) stopTouchCalibration();
            Serial.printf("CAL reset=%u\n",resetTouchAffine()); Serial.flush();
        }
        else if(!strcmp(line,"trace on")) { setTouchTraceEnabled(true); Serial.println("TRACE armed"); Serial.flush(); }
        else if(!strcmp(line,"trace off")) { stopTouchTrace(); Serial.println("TRACE disarmed"); Serial.flush(); }
        else if(!strcmp(line,"trace clear")) { bool enabled=_touchTraceEnabled; setTouchTraceEnabled(enabled); Serial.println("TRACE cleared"); Serial.flush(); }
        else if(!strcmp(line,"trace dump")) dumpTouchTrace();
        else if(!strncmp(line,"locale ",7)) { settings.data().language=atoi(line+7)==1; applySettings(); printUiState(); }
        else if(!strcmp(line,"home")) { returnHome(); printUiState(); }
        else if(!strcmp(line,"rtc")) {
            bool ok=watchClock.refresh(); auto dt=watchClock.value();
            uint8_t flags=0; auto* rtc=M5.Rtc.getRtcInstancePtr();
            bool flagRead=rtc && rtc->readRegister(0x1d,&flags,1);
            Serial.printf("RTC read=%u valid=%u state=%u flags_read=%u flags=%02x time=%04d-%02d-%02dT%02d:%02d:%02d hold=%u screen_power=%u\n",
                ok,watchClock.hasTime(),(unsigned)watchClock.state(),flagRead,flags,
                dt.date.year,dt.date.month,dt.date.date,dt.time.hours,dt.time.minutes,dt.time.seconds,
                power.rtcHoldReady(),(unsigned)_screenPowerState);
            Serial.flush();
        }
        else if(!strncmp(line,"rtc set ",8)) {
            int year,month,day,hour,minute,second; char extra;
            bool parsed=sscanf(line+8,"%d-%d-%dT%d:%d:%d %c",&year,&month,&day,&hour,&minute,&second,&extra)==6;
            bool ok=false;
            if(parsed && year>=2020 && year<=2099 && month>=1 && month<=12
                && day>=1 && day<=31 && hour>=0 && hour<=23
                && minute>=0 && minute<=59 && second>=0 && second<=59) {
                m5::rtc_datetime_t dt(m5::rtc_date_t(year,month,day,0),m5::rtc_time_t(hour,minute,second));
                ok=watchClock.set(dt);
        }
            face.invalidate(); _uiDirty=true;
            Serial.printf("RTC set=%u\n",ok); Serial.flush();
        }
        else if(!strcmp(line,"power")) {
            uint8_t cfg=0,key=0,off=0,hold=0; auto& pm=M5.Power.M5pm1;
            bool ok=pm.readRegister(0x06,&cfg,1) && pm.readRegister(0x49,&key,1) && pm.readRegister(0x4a,&off,1) && pm.readRegister(0x07,&hold,1);
            Serial.printf("POWER read=%u led_ready=%u led=%u home_ready=%u key_cfg=%02x boot_key=%02x off_cfg=%02x boot_off=%02x hold_cfg=%02x rtc_hold=%u ldo=%u\n",ok,power.indicatorReady(),(cfg&16)!=0,power.homeKeyReady(),key,power.bootKeyConfig(),off,power.bootOffConfig(),hold,power.rtcHoldReady(),(cfg&4)!=0); Serial.flush();
        } else if((line[0]=='t') && (line[1]=='d'||line[1]=='m'||line[1]=='u') && sscanf(line+2,"%d %d",&x,&y)==2) {
            if(x>=0 && x<kW && y>=0 && y<kH) handleUiPointer(line[1]=='d',line[1]!='u',line[1]=='u',x,y,millis());
            printUiState();
        } else if(!strncmp(line,"ui",2)) printUiState();
        else if(!strncmp(line,"mood ",5)) {
            int mood=atoi(line+5);
            if(mood>=0&&mood<botux::BotUx::moodCount()) showManualMood((botux::BotUx::Mood)mood);
            printUiState();
        }
        else if(line[0]=='v' || (line[0]>='0'&&line[0]<='9')) {
            selectDiagnosticPage((uint8_t)atoi(line+(line[0]=='v'))); printUiState();
        } else if(!strcmp(line,"e")) cycleExpression(1,true);
        else if(!strcmp(line,"a")) { settings.data().animation=(settings.data().animation+1)%Settings::ANIMATION_COUNT; applySettings(); settings.save(); }
        else if(!strcmp(line,"m")) { settings.data().motion=!settings.data().motion; applySettings(); settings.save(); }
        else if(!strcmp(line,"p")) pokeBot();
        else if(!strcmp(line,"s")) showStatusPanel(millis());
        else if(!strcmp(line,"c")) emitFrameCapture();
        else if(!strncmp(line,"sound ",6)) {
            int cue = atoi(line + 6);
            if (cue >= 0 && cue < (int)ux::sound::Cue::Count)
                playSound((ux::sound::Cue)cue, cueChannel((ux::sound::Cue)cue));
        }
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;
    cfg.pmic_button = true;
    M5.begin(cfg);
    M5.Touch.end(); // Raw contacts have one host gesture owner; SDK flick state is unused.
    loadTouchAffine();
    Serial.begin(115200);

    settings.begin();
    bool initialSound=soundDesired();
    _sounds.setEnabled(initialSound);
    _soundBound=_soundOutput.begin(M5.Speaker,_sounds,6,initialSound);
    power.begin();
    playSound(ux::sound::Cue::Wake, SoundChannel::Startup);
    watchClock.refresh();
    Serial.printf("RTC boot state=%u valid=%u hold=%u\n",
        (unsigned)watchClock.state(),watchClock.hasTime(),power.rtcHoldReady());

    canvas.setColorDepth(16);
    botSprite.setColorDepth(16);
    previewSprite.setColorDepth(16);
    canvas.setPsram(true);
    botSprite.setPsram(true);
    previewSprite.setPsram(false);
    bool canvasOk = canvas.createSprite(kW, kH) != nullptr;
    bool botOk = botSprite.createSprite(kBotSize, kBotSize) != nullptr;
    bool previewOk = previewSprite.createSprite(kPreviewSize, kPreviewSize) != nullptr;
    _buttonFeedbackScratch = (lgfx::rgb565_t*)heap_caps_malloc(
        kButtonScratchPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    _buttonFeedbackMasks = (uint8_t*)heap_caps_malloc(
        kButtonMasksTotal, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    _renderReady = canvasOk && botOk && previewOk
        && _buttonFeedbackScratch && _buttonFeedbackMasks;
    if (!_renderReady) {
        Serial.printf("sprite allocation failed: canvas=%d bot=%d preview=%d keys=%d masks=%d\n",
                      canvasOk, botOk, previewOk, _buttonFeedbackScratch != nullptr,
                      _buttonFeedbackMasks != nullptr);
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.drawString("DISPLAY MEMORY ERROR", kW / 2, kH / 2);
        return;
    }
    uint32_t buttonMaskMs = buildButtonFeedbackMasks();
    if (buttonMaskMs == UINT32_MAX) {
        _renderReady = false;
        Serial.println("button feedback mask decode failed");
        return;
    }

    _renderReady = face.begin(&botSprite);
    if (!_renderReady) { Serial.println("HUD allocation failed"); return; }
    previewBot.begin(&previewSprite);
    applySettings();
    uint32_t now=millis();
    refreshPower(now);
    _idleScreenPolicy.begin(now);
    _settingsCarousel.configure(kMenuCount,watchcontrols::settingsCarousel().step);
    _timeList.configure(kTimeCount,kVisibleRows);
    _personalList.configure(kPersonalCount, kVisibleRows);
    _ambientCycle.begin(millis());
    showStatusPanel(millis(), 1800);
    Serial.printf("bot-ux-watch ready; IMU=%d keys_mask_ms=%lu keys_mask_bytes=%lu. Commands: e/a/m/p/s, c capture, v0..v25 diagnostic pages, td/tm/tu x y, ui, keys 0..7/off, battery 0..100/off, trace on/off/clear/dump, cal start/repeat/direction/on/off/dump/profile/save/reset, sound N\n",
                  M5.Imu.isEnabled(), (unsigned long)buttonMaskMs,
                  (unsigned long)kButtonMasksTotal);
}

void loop() {
    M5.update();
    _soundOutput.update();
    uint32_t now = millis();
    if (!_renderReady) { delay(20); return; }

    refreshPower(now);
    bool buttonFeedbackChanged = refreshButtonFeedback(now);
    if(_clickUntil && (int32_t)(now-_clickUntil)>=0) { _clickUntil=0; _uiDirty=true; _listBandOnly=false; }
    handleInputs(now);
    if(_contactReplyPending) { _contactReplyPending=false; printUiState(); }
    handleSerialCommands();
    if(_screenPowerState==watchpower::ScreenState::Off) {
        if(!watchpower::audioSafeForLightSleep(_screenPowerState,
                                               _soundOutput.suspended())) {
            delay(8);
        } else {
            const auto config=idlePowerConfig();
            const bool forced=watchpower::forcedSleepApplies(
                config,powerClockReady(),powerClockHour());
            const bool savingBypassed=_externalPower
                &&config.keepAwakeWhileExternalPower&&!forced;
            if(watchpower::offWaitMode(_screenPowerState,
                    settings.data().wakeMode==Settings::WAKE_KEYS_ONLY,
                    savingBypassed,_gestureActive)
                  ==watchpower::OffWaitMode::ButtonLightSleep) {
                if(power.lightSleepForButtonPoll()) {
                    _manualDozing=false;
                    _idleScreenPolicy.wakeForUser(millis());
                    applyScreenPowerState(watchpower::ScreenState::Active);
                    _wakeInputGate.beginWake(false);
                    consumeWakeInput();
                }
            } else delay(8);
        }
        return;
    }
    // AMOLED wakeup includes a hardware-mandated delay. Use the current host
    // time for the RTC read and the first restored animation frame.
    now=millis();
    updateMotion(now);
    _soundOutput.update();
    bool ambientEligible=watchcompanion::ambientEligible({
        _screen==Screen::Face,!_manualPreset,!_gestureActive,!_dozing,
        !directedGazeActive(now),!_happyAcknowledgment.active(now)&&!_pokeReaction.active(now),
        _battery>kLowBattery,settings.data().expression==0
    });
    bool ambientWasPaused=_ambientCycle.paused();
    _ambientCycle.setPaused(!ambientEligible,now);
    if(ambientEligible&&ambientWasPaused) _ambientMood=botux::BotUx::Mood::Idle;
    if(ambientEligible&&_ambientCycle.update(now)) {
        _ambientMood=(botux::BotUx::Mood)(_ambientCycle.phase()==watchinteraction::AmbientCycle::Phase::LookingAround
            ?watchcompanion::lookingAroundMood():watchcompanion::ambientMood(_ambientCycle.index()));
    }
    if (_diagnosticScroll && _screen == Screen::Settings) {
        _settingsCarousel.setOffset((sinf(now*0.001f)+1)*0.5f
            *((kMenuCount-1)*watchcontrols::settingsCarousel().step));
        _menu=(MenuItem)_settingsCarousel.selected();
        markListMoved();
    }
    uint32_t scrollDt = _scrollMs ? now - _scrollMs : 0; _scrollMs = now;
    if (_screen == Screen::Settings && _settingsCarousel.update(scrollDt)) {
        _menu=(MenuItem)_settingsCarousel.selected();
        markListMoved();
    }
    if (_screen == Screen::TimeSettings && _timeScroll.update(scrollDt)) markListMoved();
    if (_screen == Screen::Personalize && _personalScroll.update(scrollDt)) markListMoved();
    if (_screen == Screen::Editor && watchcontrols::editorUsesScrollList(_editor)
        && _editorScroll.update(scrollDt)) markListMoved();

    if (_screen != Screen::Face && !_uiDirty && !previewIsAnimated()
        && !_buttonFeedbackDirty) {
        delay(2);
        return;
    }
    uint32_t interval = _dozing ? kDozeFrameMs
                      : previewIsAnimated() ? kPreviewFrameMs : kActiveFrameMs;
    if (!buttonFeedbackChanged && now - _lastFrameMs < interval) {
        delay(1);
        return;
    }
    _lastFrameMs = now;
    render(now);
}
