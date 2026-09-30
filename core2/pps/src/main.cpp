#include "main.h"
#include "lvgl_porting.h"
#include <Wire.h>
#include "PpsConsole.h"

M5ModulePPS pps;

void setup() {
  auto config = M5.config();
  config.output_power = false;
  config.internal_imu = false;
  config.internal_mic = false;
  M5.begin(config);
  Serial.begin(115200);
  M5.Power.setExtOutput(false);
  M5.Power.setBatteryCharge(false);
  M5.Display.setBrightness(96);
  Serial.printf("PPS DEMO board=%d pmic=%d sda=%d scl=%d boost=%d\n",
                M5.getBoard(), M5.Power.getType(), M5.In_I2C.getSDA(),
                M5.In_I2C.getSCL(), M5.Power.getExtOutput());
  M5.Speaker.begin();
  M5.Speaker.setVolume(100);
  M5.Display.begin();
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  while (!pps.begin(&Wire, M5.In_I2C.getSDA(), M5.In_I2C.getSCL(), MODULE_POWER_ADDR, 100000U))
  {
    M5.Display.startWrite();
    M5.Display.setTextColor(RED);
    M5.Display.drawCenterString("No I2C Dev!!!", M5.Display.width()/2, M5.Display.height()/2, &Font4);
    M5.Display.endWrite();
    ppsConsolePoll(false);
    delay(100);
  }
  pps.setPowerEnable(false);
  pps.setOutputCurrent(0.0f);
  pps.setOutputVoltage(0.0f);
  lvgl_init();
}

void loop() {
  delay(20);
}

void beep(void)
{
  if (voice_flag)
    M5.Speaker.tone(4000, 100);
}
