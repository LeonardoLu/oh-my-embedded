#include "PpsConsole.h"
#include "main.h"
#include "lvgl_porting.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
char command[64];
size_t commandLength = 0;
bool commandOverflow = false;
uint32_t lastStatusMs = 0;

void printStatus(bool ready)
{
    ready = ready && pps.getID() == 0x1041;
    if (!ready) {
        Serial.println("{\"ready\":false,\"error\":\"no_pps_at_0x35\"}");
        return;
    }
    Serial.printf("{\"ready\":true,\"id\":%u,\"enabled\":%u,\"mode\":%u,"
                  "\"vin\":%.3f,\"v_set\":%.3f,\"i_set\":%.3f,"
                  "\"vout\":%.3f,\"iout\":%.3f,\"temp\":%.1f,"
                  "\"vbus_mv\":%d,\"battery_mv\":%d,\"battery_ma\":%ld,"
                  "\"bus_boost\":%u}\n",
                  pps.getID(), pps.getPowerEnable(), pps.getMode(),
                  pps.getVIN(), pps.getOutputVoltage(), pps.getOutputCurrent(),
                  pps.getReadbackVoltage(), pps.getReadbackCurrent(),
                  pps.getTemperature(), M5.Power.getVBUSVoltage(),
                  M5.Power.getBatteryVoltage(), (long)M5.Power.getBatteryCurrent(),
                  M5.Power.getExtOutput());
}

void runCommand(bool ready)
{
    if (strcmp(command, "STATUS") == 0) {
        printStatus(ready);
        return;
    }
    if (strcmp(command, "HELP") == 0) {
        Serial.println("STATUS | SCAN | SET <volts> <amps> | ON | OFF");
        return;
    }
    if (strcmp(command, "SCAN") == 0) {
        Serial.print("I2C");
        for (uint8_t address = 1; address < 127; ++address) {
            Wire.beginTransmission(address);
            if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", address);
        }
        Serial.println();
        return;
    }
    if (!ready) {
        Serial.println("ERROR PPS not detected; check DC 9-36V and module stack");
        return;
    }

    if (strcmp(command, "OFF") == 0 || strcmp(command, "ON") == 0) {
        if (setPpsOutput(strcmp(command, "ON") == 0)) {
            updateOutputControls();
            Serial.println("OK");
        }
        printStatus(true);
        return;
    }

    float voltage, current;
    char extra;
    if (sscanf(command, "SET %f %f %c", &voltage, &current, &extra) == 2) {
        if (!isfinite(voltage) || !isfinite(current) || voltage < 0.5f ||
            voltage > 30.0f || current < 0.001f || current > 5.0f) {
            Serial.println("ERROR range: 0.5-30V, 0.001-5A");
            return;
        }
        if (!setPpsOutput(false)) return;
        updateOutputControls();
        pps.setOutputCurrent(current);
        pps.setOutputVoltage(voltage);
        syncPpsSettings();
        if (fabsf(pps.getOutputVoltage() - voltage) > 0.002f ||
            fabsf(pps.getOutputCurrent() - current) > 0.002f) {
            Serial.println("ERROR setpoint readback mismatch");
            return;
        }
        Serial.println("OK output disabled; send ON to enable");
        printStatus(true);
        return;
    }
    Serial.println("ERROR unknown command; send HELP");
}
} // namespace

bool setPpsOutput(bool enable)
{
    if (enable) {
        float vin = pps.getVIN();
        float voltage = pps.getOutputVoltage();
        float current = pps.getOutputCurrent();
        if (pps.getID() != 0x1041 || !isfinite(vin) || vin < 9.0f || vin > 36.0f) {
            Serial.println("ERROR output requires PPS 0x1041 and DC 9-36V");
            return false;
        }
        if (!isfinite(voltage) || !isfinite(current) || voltage < 0.5f ||
            voltage > 30.0f || voltage > vin - 1.0f || current < 0.001f || current > 5.0f) {
            Serial.println("ERROR set valid V/I; leave at least 1V below VIN");
            return false;
        }
    }
    pps.setPowerEnable(enable);
    if (pps.getID() != 0x1041 || pps.getPowerEnable() != enable) {
        pps.setPowerEnable(false);
        Serial.println("ERROR enable register readback mismatch; output disabled requested");
        return false;
    }
    return true;
}

void ppsConsolePoll(bool ready)
{
    // A bounded buffer avoids per-frame allocation and handles partial lines.
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            command[commandLength] = 0;
            if (commandOverflow) Serial.println("ERROR command too long");
            else if (commandLength) runCommand(ready);
            commandLength = 0;
            commandOverflow = false;
        } else if (commandLength < sizeof(command) - 1) {
            command[commandLength++] = c;
        } else {
            commandOverflow = true;
        }
    }
    if (millis() - lastStatusMs >= 1000) {
        lastStatusMs = millis();
        printStatus(ready);
    }
}
