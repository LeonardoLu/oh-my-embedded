#pragma once

// Called from the LVGL task so serial commands and touch share one I2C owner.
void ppsConsolePoll(bool ready);
bool setPpsOutput(bool enable);
void syncPpsSettings(void);
void updateOutputControls(void);
