#include "button.h"

#include <Arduino.h>

#include "pins.h"

static bool     stablePressed = false;
static bool     lastRaw = false;
static uint32_t lastChangeMs = 0;
static uint32_t pressMs = 0;

void btnInit()
{
    pinMode(PIN_BTN, INPUT_PULLUP);
    lastRaw = digitalRead(PIN_BTN) == LOW;
    // A button held through boot is not a press.
    stablePressed = lastRaw;
    pressMs = millis();
}

btn_event_t btnPoll()
{
    uint32_t now = millis();
    bool     raw = digitalRead(PIN_BTN) == LOW;

    if (raw != lastRaw)
    {
        lastRaw = raw;
        lastChangeMs = now;
        return BTN_NONE;
    }
    if (raw == stablePressed || now - lastChangeMs < BTN_DEBOUNCE_MS)
    {
        return BTN_NONE;
    }

    stablePressed = raw;
    if (stablePressed)
    {
        pressMs = now;
        return BTN_NONE;
    }
    return (now - pressMs >= BTN_LONG_PRESS_MS) ? BTN_LONG_PRESS : BTN_CLICK;
}
