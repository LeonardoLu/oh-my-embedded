#include "Settings.h"

#include <assert.h>
#include <map>
#include <string>

namespace settingstest {
std::map<std::string, bool> bools;
std::map<std::string, uint8_t> bytes;
std::map<std::string, uint16_t> ushorts;
std::map<std::string, std::string> strings;
}

void botux::BotUx::setStyle(const Style& style) {
    _style = style;
}

int main() {
    Settings defaults;
    assert(defaults.data().sound);
    assert(defaults.data().startupSound);
    assert(defaults.data().buttonSound);
    assert(defaults.data().alertSound);
    assert(defaults.data().powerSaveEnabled);
    assert(defaults.data().dimBrightness == 1);
    assert(defaults.data().dimTimeout == 1);
    assert(defaults.data().screenOffTimeout == 2);
    assert(defaults.data().wakeMode == Settings::WAKE_TOUCH_KEYS);
    assert(defaults.data().keepAwakeWhileCharging);
    assert(!defaults.data().forcedSleepEnabled);
    assert(defaults.data().forcedSleepStartHour == 23);
    assert(defaults.data().forcedSleepEndHour == 7);
    assert(Settings::TIMEOUT_COUNT == 6);
    assert(Settings::timeoutIndex(0, 1) == 0);
    assert(Settings::timeoutIndex(5, 1) == 5);
    assert(Settings::timeoutIndex(6, 1) == 1);

    // An old preference set has no new keys and keeps its existing values.
    settingstest::bools["hour24"] = false;
    settingstest::bools["sound"] = false;
    settingstest::bools["buttonFx"] = false;
    settingstest::bytes["theme"] = Settings::THEME_MONO;
    Settings oldNvs;
    oldNvs.begin();
    assert(!oldNvs.data().hour24);
    assert(!oldNvs.data().sound);
    assert(oldNvs.data().startupSound);
    assert(oldNvs.data().buttonSound);
    assert(oldNvs.data().alertSound);
    assert(!oldNvs.data().buttonFeedback);
    assert(oldNvs.data().theme == Settings::THEME_MONO);
    assert(oldNvs.data().dimTimeout == 1);
    assert(oldNvs.data().screenOffTimeout == 2);
    assert(oldNvs.data().wakeMode == Settings::WAKE_TOUCH_KEYS);
    assert(oldNvs.data().keepAwakeWhileCharging);

    // The old boolean wake key migrates when the new wake-mode key is absent.
    settingstest::bools["buttonWake"] = true;
    Settings migrated;
    migrated.begin();
    assert(migrated.data().wakeMode == Settings::WAKE_KEYS_ONLY);

    // The unpublished inverse preference still migrates safely if it exists.
    settingstest::bools["chargeSave"] = true;
    Settings migratedCharge;
    migratedCharge.begin();
    assert(!migratedCharge.data().keepAwakeWhileCharging);

    // Corrupt indices and clock hours recover independently to shipped defaults.
    settingstest::bytes["dimTimeout"] = 6;
    settingstest::bytes["screenOffTo"] = 255;
    settingstest::bytes["dimBright"] = 0;
    settingstest::bytes["wakeMode"] = 255;
    settingstest::bytes["sleepStart"] = 24;
    settingstest::bytes["sleepEnd"] = 255;
    Settings repaired;
    repaired.begin();
    assert(repaired.data().dimBrightness == 1);
    assert(repaired.data().dimTimeout == 1);
    assert(repaired.data().screenOffTimeout == 2);
    assert(repaired.data().wakeMode == Settings::WAKE_TOUCH_KEYS);
    assert(repaired.data().forcedSleepStartHour == 23);
    assert(repaired.data().forcedSleepEndHour == 7);

    settingstest::bytes["bright"] = 2;
    settingstest::bytes["dimBright"] = 5;
    Settings capped;
    capped.begin();
    assert(capped.data().brightness == 2);
    assert(capped.data().dimBrightness == 2);

    // All selections and independent switches survive a save/load cycle.
    for (uint8_t index = 0; index < Settings::TIMEOUT_COUNT; ++index) {
        repaired.data().sound = (index & 1) == 0;
        repaired.data().startupSound = (index & 1) != 0;
        repaired.data().buttonSound = (index & 2) != 0;
        repaired.data().alertSound = (index & 4) != 0;
        repaired.data().powerSaveEnabled = (index & 1) != 0;
        repaired.data().brightness = Settings::BRIGHTNESS_MAX;
        repaired.data().dimBrightness = (uint8_t)(1 + index % 5);
        repaired.data().dimTimeout = index;
        repaired.data().screenOffTimeout = Settings::TIMEOUT_COUNT - 1 - index;
        repaired.data().wakeMode = (index & 1) != 0
            ? Settings::WAKE_KEYS_ONLY : Settings::WAKE_TOUCH_KEYS;
        repaired.data().keepAwakeWhileCharging = (index & 2) != 0;
        repaired.data().forcedSleepEnabled = (index & 1) == 0;
        repaired.data().forcedSleepStartHour = index;
        repaired.data().forcedSleepEndHour = (uint8_t)(23 - index);
        repaired.save();
        assert(settingstest::bools["sound"] == ((index & 1) == 0));
        assert(settingstest::bools["startupSound"] == ((index & 1) != 0));
        assert(settingstest::bools["buttonSound"] == ((index & 2) != 0));
        assert(settingstest::bools["alertSound"] == ((index & 4) != 0));
        assert(settingstest::bools["powerSave"] == ((index & 1) != 0));
        assert(settingstest::bytes["dimBright"] == (uint8_t)(1 + index % 5));
        assert(settingstest::bytes["dimTimeout"] == index);
        assert(settingstest::bytes["screenOffTo"] == Settings::TIMEOUT_COUNT - 1 - index);
        assert(settingstest::bytes["wakeMode"] == (index & 1));
        assert(settingstest::bools["buttonWake"] == ((index & 1) != 0));
        assert(settingstest::bools["chargeAwake"] == ((index & 2) != 0));
        assert(settingstest::bools["chargeSave"] == ((index & 2) == 0));
        assert(settingstest::bools["forceSleep"] == ((index & 1) == 0));
        assert(settingstest::bytes["sleepStart"] == index);
        assert(settingstest::bytes["sleepEnd"] == (uint8_t)(23 - index));

        Settings restored;
        restored.begin();
        assert(restored.data().sound == ((index & 1) == 0));
        assert(restored.data().startupSound == ((index & 1) != 0));
        assert(restored.data().buttonSound == ((index & 2) != 0));
        assert(restored.data().alertSound == ((index & 4) != 0));
        assert(restored.data().powerSaveEnabled == ((index & 1) != 0));
        assert(restored.data().dimBrightness == (uint8_t)(1 + index % 5));
        assert(restored.data().dimTimeout == index);
        assert(restored.data().screenOffTimeout == Settings::TIMEOUT_COUNT - 1 - index);
        assert(restored.data().wakeMode == (index & 1));
        assert(restored.data().keepAwakeWhileCharging == ((index & 2) != 0));
        assert(restored.data().forcedSleepEnabled == ((index & 1) == 0));
        assert(restored.data().forcedSleepStartHour == index);
        assert(restored.data().forcedSleepEndHour == (uint8_t)(23 - index));
    }
}
