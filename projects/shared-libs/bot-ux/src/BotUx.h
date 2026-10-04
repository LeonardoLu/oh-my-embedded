// BotUx — a small, controllable Grok-inspired orb animation component.
//
// The component renders a fluid orb with two expressive eye marks into a
// caller-provided M5Canvas sprite. It never owns the display: the host creates
// the sprite, calls begin(), drives update()/draw() each frame, and pushes the
// sprite to its display when it pleases. This keeps the component embeddable
// into both the M5Stack StopWatch and Core2 with zero display
// coupling.
//
// Animation is time-based (millis), framerate-agnostic, and driven by small
// state machines for blinking, gaze, body motion, speech, and reactions.
#pragma once

#include <M5GFX.h> // M5Canvas (== LGFX_Sprite); lighter than pulling all of M5Unified
#include <stdint.h>
#include <stddef.h>

namespace botux {

// ---- color helpers -------------------------------------------------------
// M5GFX stores pixels as RGB565. This keeps defaults self-contained and avoids
// depending on the TFT_* macro set.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

class BotUx {
public:
    enum class Mood : uint8_t {
        Idle = 0,   // canonical upper-right gaze, breathing, periodic blinks
        Listening,  // engaged, eyes widen, subtle forward nod
        Thinking,   // breathing volumetric dot shell
        Speaking,   // body pulse follows setTalking()
        Happy,      // crescent eyes, blush and a buoyant celebratory lift
        Sad,        // lowered gaze and settled body
        Sleepy,     // half/closed eyes, slow breath
        Surprised,  // wide eye marks + quick vertical stretch (transient)
        Working,    // flowing vortex dot shell
        Waiting,    // alert, patient eyes with a slow searching glance
        Blocked,    // exclamation-mark silhouette
        Done,       // settled orb with a low-left glance and completion check
        Asleep,     // closed eyes, deep breathing and drifting zzz (append-only)
        LookingAround, // full-range autonomous gaze shared with touch/directions
    };

    enum class EyeStyle : uint8_t {
        Round = 0, // short rounded eye marks
        Oval,      // long Grok-like pill marks
        Square,    // broad rounded marks
        Googly,    // round marks with free-floating pupils
    };

    enum class BodyStyle : uint8_t {
        None = 0,        // eye marks only (no orb silhouette)
        Round,           // classic fluid orb
        RoundedSquare,   // soft, broad blob
        Hexagon,         // lightly faceted orb
    };

    // Face pose can follow Mood (Auto) or be selected independently. This is
    // useful for an appearance preview and for brief host-driven reactions.
    enum class Expression : uint8_t {
        Auto = 0,
        Neutral,
        Curious,
        Focused,
        Joy,
        Skeptical,
        Bashful,
        Wink,
        Dizzy,
        Alarmed,
    };

    // Persistent choreography layered over the current mood/expression.
    // Auto chooses the lifecycle motion associated with the effective mood.
    enum class Animation : uint8_t {
        Auto = 0,
        Calm,
        Curious,
        Orbit,
        Bounce,
        Glitch,
        Wave,
        Sparkle,
    };

    enum class GazeDirection : uint8_t { Auto = 0, Center, Left, Right, Up, Down, UpLeft, UpRight, DownLeft, DownRight };
    // Auto mirrors facial handedness from the eye group's horizontal half.
    // Explicit sides select the same authored expression on demand without
    // reflecting the body or framebuffer.
    enum class FaceSide : uint8_t { Auto = 0, Left, Right };
    enum class Language : uint8_t { English = 0, Chinese };
    static constexpr size_t kNameMax = 16;
    static constexpr uint8_t gazeDirectionCount() { return 10; }
    static constexpr uint8_t explicitGazeDirectionCount() { return 9; }
    static GazeDirection explicitGazeDirection(uint8_t index) {
        return static_cast<GazeDirection>(1 + index % explicitGazeDirectionCount());
    }
    static const char* gazeDirectionName(GazeDirection value, Language language = Language::English);
    // Persistent exact direction. Auto follows the mood/expression gaze.
    // Temporary gazeAt takes precedence, then returns here. No UI/persistence ownership.
    void setGazeDirection(GazeDirection direction);
    GazeDirection gazeDirection() const { return _gazeDirection; }
    void setFaceSide(FaceSide side);
    FaceSide faceSide() const { return _faceSide; }
    FaceSide effectiveFaceSide() const { return _faceHandedness < 0.0f ? FaceSide::Left : FaceSide::Right; }
    static constexpr uint8_t moodCount() { return 14; }
    static constexpr uint8_t expressionCount() { return 10; }
    static constexpr uint8_t animationCount() { return 8; }
    static const char* moodName(Mood value, Language language = Language::English);
    static const char* expressionName(Expression value, Language language = Language::English);
    static const char* animationName(Animation value, Language language = Language::English);
    // ASCII letters/digits/spaces/hyphens/apostrophes; trims spaces, empty => Milo.
    void setName(const char* name);
    const char* name() const { return _name; }
    Mood mood() const { return _mood; }
    Mood effectiveMood() const { return _effMood; }
    Expression effectiveExpression() const { return _effExpression; }
    // Natural named caption: effective mood, or explicit expression while Idle.
    // Bounded, NUL-terminated UTF-8; returns required byte count (excluding NUL).
    size_t describe(char* out, size_t capacity, Language language = Language::English) const;
    struct Preset { Mood mood; Expression expression; Animation animation; };
    static constexpr uint8_t presetCount() { return 8; }
    static Preset preset(uint8_t index);
    void applyPreset(uint8_t index);
    uint8_t randomPreset();
    uint8_t nextPreset();
    void resetToIdle();
    // Normalized canvas direction [-1,1], positive y down. Accepted in Idle
    // and LookingAround. Temporary targets ease in, then resume the mood pose;
    // 0 cancels the hold.
    void gazeAt(float x, float y, uint16_t holdMs = 1800);
    void clearGaze();

    // Everything that makes one bot "this bot".
    struct Style {
        uint16_t bgColor      = rgb565(0x0A, 0x0E, 0x14); // deep space blue
        uint16_t bodyColor    = rgb565(0xF4, 0xF6, 0xF8); // orb surface
        uint16_t accentColor  = rgb565(0x66, 0xAE, 0xFF); // status / secondary UI
        uint16_t eyeColor     = rgb565(0x20, 0x24, 0x29); // graphite eye marks
        uint16_t pupilColor   = rgb565(0x08, 0x2B, 0x55); // googly pupils
        uint16_t mouthColor   = rgb565(0x20, 0x24, 0x29);
        uint16_t blushColor   = rgb565(0xFF, 0x8A, 0xA0);

        EyeStyle eyeStyle   = EyeStyle::Oval;
        BodyStyle bodyStyle = BodyStyle::Round;

        float    eyeSize    = 1.0f;   // 0.5..2.0, scales eye radius
        uint32_t blinkMinMs = 2200;   // min time between blinks
        uint32_t blinkMaxMs = 5200;   // max time between blinks
    };

    // Computed layout, exposed so hosts can position overlays (e.g. a clock
    // label, battery bar) relative to the face.
    struct Metrics {
        int16_t cx = 0, cy = 0;   // face centre
        int16_t eyeY = 0;         // eye baseline
        int16_t eyeDX = 0;        // horizontal half-distance between eyes
        int16_t eyeRadius = 0;    // base eye radius (px)
        int16_t mouthY = 0;       // mouth baseline
        int16_t bodyR = 0;        // body radius (0 if BodyStyle::None)
    };

    BotUx() = default;

    // Bind a render target. The sprite must already be created (createSprite)
    // at 16-bit RGB565; the component reads its size from the canvas. The canvas
    // must outlive the BotUx.
    void begin(M5Canvas* canvas);

    // ---- configuration ----------------------------------------------------
    void setStyle(const Style& s);
    const Style& style() const { return _style; }

    // Persistent mood. Transient reactions (poke) override for their duration
    // then return to this mood.
    void setMood(Mood m);
    void setMood(Mood m, uint16_t transitionMs);

    // Select a face pose. transitionMs=0 snaps on the next update; other
    // values ease to the new geometry without depending on frame rate.
    void setExpression(Expression expression, uint16_t transitionMs = 180);
    Expression expression() const { return _expression; }

    // Animation settings are deliberately scalar so device settings screens
    // can expose them without owning animation details.
    void setAnimation(Animation animation);
    Animation animation() const { return _animation; }
    void setAnimationSpeed(float speed);       // 0.25..3.0, default 1.0
    float animationSpeed() const { return _animationSpeed; }
    void setMotionAmount(float amount);        // 0..2, default 1.0
    float motionAmount() const { return _motionAmount; }
    void setReducedMotion(bool reduced);
    bool reducedMotion() const { return _reducedMotion; }

    // Drive the body pulse while Speaking.
    void setTalking(bool on);

    // 0..100 battery. <=10% forces a sleepy droop regardless of mood.
    // Setting a value also makes the battery icon visible.
    void setBattery(uint8_t pct);
    void setBatteryVisible(bool vis); // show/hide the icon independently

    // Signal strength: -1 hides, 0..4 shows compact top-left bars.
    void setSignal(int8_t bars);

    // Optional clock text drawn under the face (e.g. "09:41"). h is 1..12; pm
    // selects AM/PM formatting (only used to echo an "AM/PM" suffix by the host).
    void setTime(uint8_t h, uint8_t m, uint8_t s, bool pm);
    // Single-line caption, nullptr hides. The string must stay valid until the
    // next setLabel() or BotUx destruction — pass a static/persistent buffer,
    // never a String().c_str() or stack temporary.
    void setLabel(const char* text);

    // ---- interaction ------------------------------------------------------
    // Surprise for ~240ms, settle through Happy, then return to the prior mood.
    void poke();

    // Feed normalized IMU input. tiltX/tiltY are -1..1 and shake is 0..1.
    // Inputs are smoothed in update(); hosts may call this at any sensor rate.
    void setMotion(float tiltX, float tiltY, float shake = 0.0f);

    // ---- frame loop -------------------------------------------------------
    // Advance animation. Pass 0 (or nothing) to use millis() internally.
    void update(uint32_t nowMs = 0);
    void draw(); // render the current frame into the bound canvas

    const Metrics& metrics() const { return _m; }

    // Nudge blink and gaze timing (e.g. from ADC) so bots do not move in lockstep.
    void seedBlink(uint32_t s);

private:
    // animation state
    void _updateBlink(uint32_t now);
    void _updateGaze(uint32_t now);
    void _updateTalk(uint32_t now);
    void _resolveMood(uint32_t now); // set _effMood + _open from mood/reaction/battery

    float _smooth(float t) const { return t * t * (3.0f - 2.0f * t); } // smoothstep
    float _pulse(uint32_t ms, float period, float phase = 0.0f) const; // 0..1

    void _drawBody();
    void _drawEyes();
    void _drawAnimationFx();
    void _drawOverlays(); // battery/signal/label/time
    void _fillCapsule(float cx, float cy, float halfDx, float halfDy,
                      float radius, uint16_t color, uint8_t opacity = 255);
    void _fillSparkle(float cx, float cy, float verticalRadius,
                      float horizontalRadius, uint16_t color);
    void _fillEyeCurve(float cx, float cy, float dx, float dy, float rise,
                       float radius, uint16_t color);
    void _fillEllipseAA(float cx, float cy, float rx, float ry, uint16_t color);

    M5Canvas* _cv = nullptr;
    int16_t _w = 0, _h = 0;
    Style _style;
    Metrics _m;

    char _name[kNameMax + 1] = "Milo";
    uint8_t _presetIndex = 0;
    uint32_t _presetSeed = 0xA341316Cu;
    GazeDirection _gazeDirection = GazeDirection::Auto;
    bool _gazeHeld = false;
    uint32_t _gazeUntil = 0;
    float _gazeX = 0.0f, _gazeY = 0.0f;
    float _directionPose = 0.0f; // eased perspective weight shared by direction and tap
    Mood _mood = Mood::Idle;
    Expression _expression = Expression::Auto;
    Animation _animation = Animation::Auto;
    Animation _activeAnimation = Animation::Calm;
    float _animationSpeed = 1.0f;
    float _motionAmount = 1.0f;
    bool _reducedMotion = false;
    uint16_t _transitionMs = 180;
    bool _snapPose = false;

    bool   _talking = false;
    uint8_t _battery = 100;  // 0..100
    bool   _batteryVisible = false;
    int8_t  _signal = -1;    // -1 hidden, 0..4
    bool   _hasTime = false;
    uint8_t _hh = 0, _mm = 0, _ss = 0;
    bool   _pm = false;
    const char* _label = nullptr;

    // per-frame derived state (computed in update(), consumed by draw())
    uint32_t _now = 0;
    uint32_t _lastNow = 0;   // previous update() timestamp, for framerate-independent easing
    Mood   _effMood = Mood::Idle;
    Expression _effExpression = Expression::Neutral;
    float  _openBase = 1.0f; // eased base openness (mood/sleep), before blink
    float  _open = 1.0f;     // eye openness 0..1 (blink + sleep + mood)
    float  _pupilDX = 0.0f;  // eased gaze, -1..1
    float  _pupilDY = 0.0f;
    float  _talkAmp = 0.0f;  // 0..1 speech/body pulse amplitude
    float  _breath = 0.0f;   // -1..1 body breathing
    float  _bodyDX = 0.0f;   // animated pose in pixels
    float  _bodyDY = 0.0f;
    float  _bodySX = 1.0f;
    float  _bodySY = 1.0f;
    float  _bodyLean = 0.0f; // -1..1; offsets the two eyes vertically
    float  _eyeSmile = 0.0f;
    float  _eyeWink = 0.0f;
    float  _eyeRound = 0.0f;
    float  _eyeAsym = 0.0f;  // -1..1; closes one eye for thought
    float  _eyePairX = 0.26f; // pair centre, fractions of body radius
    float  _eyePairY = -0.38f;
    float  _eyeAngle = 0.34f; // horizontal / vertical major-axis ratio
    float  _eyeTwist = 0.0f;  // expression rotation of the eye pair
    float  _animEyeTwist = 0.0f;

    // Smoothed normalized IMU state.
    float _motionTargetX = 0.0f;
    float _motionTargetY = 0.0f;
    float _shakeTarget = 0.0f;
    float _motionX = 0.0f;
    float _motionY = 0.0f;
    float _shake = 0.0f;

    // LookingAround changes full-range targets at irregular hold intervals.
    float    _wanderX = 0.0f;
    float    _wanderY = 0.0f;
    uint32_t _nextGaze = 0;
    uint32_t _gazeSeed = 0x9E3779B9u;
    FaceSide _faceSide = FaceSide::Auto;
    float _faceHandedness = 1.0f; // -1 left .. +1 right, eased through crossings

    // blink
    uint32_t _blinkSeed = 0x1234;
    uint32_t _nextBlink = 0;
    uint32_t _blinkStart = 0;
    bool     _blinking = false;
    uint8_t  _blinkPhase = 0; // 0 closing, 1 closed, 2 opening

    // reactions
    uint32_t _reactionUntil = 0; // happy phase end (0 = none)
    uint32_t _pokeUntil = 0;     // surprise phase end

    uint32_t _animStart = 0;     // fixed origin for deterministic drift
    uint32_t _talkSeed = 0;
};

} // namespace botux
