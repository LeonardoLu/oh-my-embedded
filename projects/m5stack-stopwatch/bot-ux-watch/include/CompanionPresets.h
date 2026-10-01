#pragma once
#include <stdint.h>

namespace watchcompanion {

class MoodDeck {
public:
    explicit MoodDeck(uint8_t count, uint32_t seed=0xC01D5EEDu):_count(count),_seed(seed) {}

    uint8_t next(uint8_t current) const {
        return _count?(uint8_t)((current+1)%_count):0;
    }

    uint8_t random(uint8_t current) {
        if(_count<2) return 0;
        _seed=_seed*1664525u+1013904223u;
        return (uint8_t)((current+1+((_seed>>16)%(_count-1)))%_count);
    }

private:
    uint8_t _count;
    uint32_t _seed;
};

// Quiet autonomous states only. Interactive buttons can still select every mood.
static const uint8_t kAmbientMoods[]={0,1,2,4,8,9,11};
constexpr uint8_t ambientMoodCount() { return sizeof(kAmbientMoods)/sizeof(kAmbientMoods[0]); }
inline uint8_t ambientMood(uint8_t index) { return kAmbientMoods[index%ambientMoodCount()]; }
constexpr uint8_t lookingAroundMood() { return 13; }

struct AmbientAvailability {
    bool face;
    bool automatic;
    bool gestureFree;
    bool awake;
    bool gazeClear;
    bool transientClear;
    bool batteryOkay;
    bool expressionAuto;
};

inline bool ambientEligible(const AmbientAvailability& value) {
    return value.face&&value.automatic&&value.gestureFree&&value.awake&&value.gazeClear
        &&value.transientClear&&value.batteryOkay&&value.expressionAuto;
}

struct Presentation {
    uint8_t expression;
    uint8_t animation;
    bool talking;
};

inline Presentation presentation(bool gazeActive,bool manual,uint8_t manualMood,
                                 uint8_t manualExpression,uint8_t manualAnimation,
                                 uint8_t savedExpression,uint8_t savedAnimation,
                                 bool forceAutoExpression) {
    if(gazeActive) return {0,0,false};
    if(manual) return {manualExpression,manualAnimation,manualMood==3};
    return {(uint8_t)(forceAutoExpression?0:savedExpression),savedAnimation,false};
}

}
