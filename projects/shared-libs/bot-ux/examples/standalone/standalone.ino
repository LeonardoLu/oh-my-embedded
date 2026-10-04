// Standalone bot-ux demo. Builds for any M5Stack board (StickC Plus2, Core2, …).
#include <M5Unified.h>
#include <BotUx.h>

M5Canvas canvas(&M5.Display);
botux::BotUx bot;

static const botux::BotUx::Mood kMoods[] = {
    botux::BotUx::Mood::Idle,     botux::BotUx::Mood::Listening,
    botux::BotUx::Mood::Thinking, botux::BotUx::Mood::Speaking,
    botux::BotUx::Mood::Happy,    botux::BotUx::Mood::Sad,
    botux::BotUx::Mood::Sleepy,   botux::BotUx::Mood::Surprised,
    botux::BotUx::Mood::Working,  botux::BotUx::Mood::Waiting,
    botux::BotUx::Mood::Blocked,  botux::BotUx::Mood::Done,
    botux::BotUx::Mood::Asleep,   botux::BotUx::Mood::LookingAround,
};
static int moodIdx = 0;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    canvas.setColorDepth(16);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    bot.begin(&canvas);
    bot.setAnimation(botux::BotUx::Animation::Auto);
    bot.setAnimationSpeed(1.0f);
    bot.setMotionAmount(1.0f);
    bot.setBattery(87);
    bot.setBatteryVisible(false);
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        bot.poke(); // surprise -> happy
    }
    if (M5.BtnB.wasPressed()) {
        moodIdx = (moodIdx + 1) % (sizeof(kMoods) / sizeof(kMoods[0]));
        bot.setMood(kMoods[moodIdx]);
    }

    bot.update(millis());
    bot.draw();
    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay();
}
