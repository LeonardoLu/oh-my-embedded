#include "CompanionPresets.h"
#include <assert.h>

int main() {
    watchcompanion::MoodDeck deck(14,0x12345678u);
    for(uint8_t mood=0;mood<14;++mood) assert(deck.next(mood)==(mood+1)%14);
    bool seen[14]={}; uint8_t current=0;
    for(int i=0;i<1024;++i) {
        uint8_t next=deck.random(current);
        assert(next<14&&next!=current); seen[next]=true; current=next;
    }
    for(bool value:seen) assert(value);
    const uint8_t expected[]={0,1,2,4,8,9,11};
    static_assert(watchcompanion::ambientMoodCount()==7,"safe ambient mood count");
    for(uint8_t i=0;i<watchcompanion::ambientMoodCount();++i)
        assert(watchcompanion::ambientMood(i)==expected[i]);
    static_assert(watchcompanion::lookingAroundMood()==13,"append-only LookingAround mood");

    watchcompanion::AmbientAvailability available={true,true,true,true,true,true,true,true};
    assert(watchcompanion::ambientEligible(available));
    bool* fields[]={&available.face,&available.automatic,&available.gestureFree,&available.awake,
        &available.gazeClear,&available.transientClear,&available.batteryOkay,&available.expressionAuto};
    for(bool* field:fields) {
        *field=false;
        assert(!watchcompanion::ambientEligible(available));
        *field=true;
    }

    // A center touch temporarily clears saved Focused/Wave, then restores both.
    auto gaze=watchcompanion::presentation(true,false,0,0,0,3,6,false);
    assert(gaze.expression==0&&gaze.animation==0&&!gaze.talking);
    auto restored=watchcompanion::presentation(false,false,0,0,0,3,6,false);
    assert(restored.expression==3&&restored.animation==6&&!restored.talking);
    auto speaking=watchcompanion::presentation(false,true,3,0,0,8,6,false);
    assert(speaking.expression==0&&speaking.animation==0&&speaking.talking);
    auto interrupted=watchcompanion::presentation(true,true,3,0,0,8,6,false);
    assert(!interrupted.talking);
    // Looking around remains a held manual selection; touch only overrides its
    // presentation for the same temporary Idle gaze used by every other mood.
    auto manualLooking=watchcompanion::presentation(false,true,13,4,5,8,6,false);
    assert(manualLooking.expression==4&&manualLooking.animation==5&&!manualLooking.talking);
    auto touchedLooking=watchcompanion::presentation(true,true,13,4,5,8,6,false);
    assert(touchedLooking.expression==0&&touchedLooking.animation==0&&!touchedLooking.talking);
}
