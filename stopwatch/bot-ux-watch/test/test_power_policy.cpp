#include "PowerPolicy.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

using watchpower::IdleScreenPolicy;
using watchpower::ScreenState;
using watchpower::OffWaitMode;
using watchpower::WakeInputGate;

struct FakePmic {
    uint8_t regs[8]={};
    bool failRead=false,failWrite=false,ignoreWrite=false;
    int writes=0;
    bool readRegister(uint8_t reg,uint8_t* out,int) {
        if(failRead) return false;
        *out=regs[reg]; return true;
    }
    bool writeRegister8(uint8_t reg,uint8_t value) {
        ++writes;
        if(failWrite) return false;
        if(!ignoreWrite) regs[reg]=value;
        return true;
    }
};

int main() {
    FakePmic pm;
    pm.regs[6]=0x1b; pm.regs[7]=0x45;
    assert(watchpower::retainRtcPower(pm));
    assert(pm.regs[6]==0x1f && pm.regs[7]==0x65);
    assert(watchpower::retainRtcPower(pm) && pm.writes==2);
    pm.regs[7]=0; pm.failWrite=true;
    assert(!watchpower::retainRtcPower(pm));
    pm.failWrite=false; pm.ignoreWrite=true;
    assert(!watchpower::retainRtcPower(pm));
    pm.ignoreWrite=false;
    assert(watchpower::retainRtcPower(pm));
    pm.failRead=true;
    assert(!watchpower::retainRtcPower(pm));
    assert(watchpower::kTimeoutCount == 6);
    const uint32_t expected[] = {5000, 15000, 60000, 300000, 600000, 900000};
    for (uint8_t i = 0; i < watchpower::kTimeoutCount; ++i)
        assert(watchpower::timeoutMs(i) == expected[i]);
    assert(watchpower::timeoutMs(255) == expected[5]);
    assert(strcmp(watchpower::timeoutLabel(0),"5 S")==0);
    assert(strcmp(watchpower::timeoutLabel(5),"15 MIN")==0);

    IdleScreenPolicy policy;
    policy.begin(1000);
    assert(policy.update(15999, false, false, 1, 2) == ScreenState::Active);
    assert(policy.update(16000, false, false, 1, 2) == ScreenState::Dimmed);
    assert(policy.update(60999, false, false, 1, 2) == ScreenState::Dimmed);
    assert(policy.update(61000, false, false, 1, 2) == ScreenState::Off);

    // Each timeout is independent: screen-off wins when configured sooner.
    policy.begin(0);
    assert(policy.update(5000, false, false, 5, 0) == ScreenState::Off);

    // Held input keeps resetting inactivity; release starts a new full interval.
    policy.begin(0);
    assert(policy.update(60000, false, true, 0, 1) == ScreenState::Active);
    assert(policy.lastActivity() == 60000);
    assert(policy.update(64999, false, false, 0, 1) == ScreenState::Active);
    assert(policy.update(65000, false, false, 0, 1) == ScreenState::Dimmed);

    // External power is always active and unplug starts fresh from the last poll.
    policy.begin(0);
    assert(policy.update(900000, true, false, 0, 1) == ScreenState::Active);
    assert(policy.update(904999, false, false, 0, 1) == ScreenState::Active);
    assert(policy.update(905000, false, false, 0, 1) == ScreenState::Dimmed);

    // Unsigned elapsed arithmetic remains correct across millis() rollover.
    policy.begin(UINT32_MAX - 2000U);
    assert(policy.update(2998, false, false, 0, 1) == ScreenState::Active);
    assert(policy.update(2999, false, false, 0, 1) == ScreenState::Dimmed);
    policy.wake(123);
    assert(policy.state() == ScreenState::Active);
    assert(policy.lastActivity() == 123);

    assert(watchpower::offWaitMode(ScreenState::Off,true,false,false)
           ==OffWaitMode::ButtonLightSleep);
    assert(watchpower::offWaitMode(ScreenState::Off,false,false,false)
           ==OffWaitMode::AwakePoll);
    assert(watchpower::offWaitMode(ScreenState::Off,true,false,true)
           ==OffWaitMode::AwakePoll);
    assert(watchpower::offWaitMode(ScreenState::Off,true,true,false)
           ==OffWaitMode::AwakePoll);
    assert(watchpower::shouldWakeAndConsume(ScreenState::Dimmed,true));
    assert(watchpower::shouldWakeAndConsume(ScreenState::Off,true));
    assert(!watchpower::shouldWakeAndConsume(ScreenState::Active,true));
    assert(!watchpower::shouldWakeAndConsume(ScreenState::Off,false));

    // Sound hardware runs only while the screen is fully active. Light sleep
    // additionally waits for a positive hardware-suspended acknowledgement.
    assert(watchpower::audioShouldRun(ScreenState::Active,true));
    assert(!watchpower::audioShouldRun(ScreenState::Active,false));
    assert(!watchpower::audioShouldRun(ScreenState::Dimmed,true));
    assert(!watchpower::audioShouldRun(ScreenState::Off,true));
    assert(watchpower::audioSafeForLightSleep(ScreenState::Off,true));
    assert(!watchpower::audioSafeForLightSleep(ScreenState::Off,false));
    assert(!watchpower::audioSafeForLightSleep(ScreenState::Dimmed,true));
    assert(!watchpower::audioSafeForLightSleep(ScreenState::Active,true));

    // A brief EXT1 key pulse can be released before the next M5.update. The
    // synthetic wake is consumed for one cycle and then controls reopen.
    WakeInputGate brief;
    brief.beginWake(false);
    assert(brief.update(100,false,false,false).blockControls);
    assert(!brief.releasePending());
    assert(!brief.update(101,false,false,false).blockControls);

    // A key held through wake stays blocked through the release sample.
    WakeInputGate held;
    held.beginWake(false);
    assert(held.update(200,true,false,false).blockControls);
    assert(held.releasePending());
    assert(held.update(220,false,false,false).blockControls);
    assert(!held.update(221,false,false,false).blockControls);

    // A raw PMIC wake and its delayed SDK click are one consumed action.
    WakeInputGate power;
    power.beginWake(true);
    assert(power.update(300,true,true,false).blockControls);
    assert(power.update(400,false,false,false).blockControls);
    auto delayed=power.update(800,false,false,true);
    assert(!delayed.blockControls&&delayed.consumedPowerClick);
    assert(!power.powerClickPending());
    assert(!power.update(801,false,false,true).consumedPowerClick);

    // Cable insertion and a simultaneous PMIC press use the same gate.
    WakeInputGate cableAndPower;
    cableAndPower.beginWake(true);
    assert(cableAndPower.update(1000,true,true,false).blockControls);
    assert(cableAndPower.update(1100,false,false,true).consumedPowerClick);
    assert(!cableAndPower.powerClickPending());

    // An SDK click already used as the wake event has no later duplicate to
    // suppress; a subsequent intentional click remains available.
    WakeInputGate deliveredClick;
    deliveredClick.beginWake(false);
    assert(deliveredClick.update(1200,false,false,true).blockControls);
    auto nextClick=deliveredClick.update(1201,false,false,true);
    assert(!nextClick.blockControls&&!nextClick.consumedPowerClick);
    return 0;
}
