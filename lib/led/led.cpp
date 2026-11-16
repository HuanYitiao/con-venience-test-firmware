#include "led.h"

#include <Arduino.h>

#include "pins.h"

#define FLASH_ON_MS 120
#define FLASH_OFF_MS 150
#define BREATHE_PERIOD_MS 3000

static led_color_t   color = {0, 0, 0};
static led_pattern_t pattern = LED_OFF;
static uint32_t      startMs = 0;
static int16_t       lastLevel = -1;  // -1 forces the next write
static bool          done = true;

static void write(uint8_t level)
{
    if (level == lastLevel)
    {
        return;
    }
    lastLevel = level;
    uint16_t scale = (uint16_t)level * LED_BRIGHTNESS / 255;
    rgbLedWriteOrdered(PIN_LED, LED_COLOR_ORDER, color.r * scale / 255, color.g * scale / 255,
                       color.b * scale / 255);
}

void ledInit()
{
    lastLevel = -1;
    write(0);
}

void ledSet(led_color_t c, led_pattern_t p)
{
    color = c;
    pattern = p;
    startMs = millis();
    done = false;
    lastLevel = -1;
    ledTick();
}

static uint8_t flashLevel(uint32_t t, int count)
{
    uint32_t period = FLASH_ON_MS + FLASH_OFF_MS;
    if (t >= period * count)
    {
        done = true;
        return 0;
    }
    return (t % period) < FLASH_ON_MS ? 255 : 0;
}

void ledTick()
{
    uint32_t t = millis() - startMs;
    uint8_t  level = 0;

    switch (pattern)
    {
        case LED_OFF:
            done = true;
            level = 0;
            break;
        case LED_SOLID:
            level = 255;
            break;
        case LED_BREATHE:
        {
            // Triangle wave, squared for a softer bottom.
            uint32_t phase = t % BREATHE_PERIOD_MS;
            uint32_t half = BREATHE_PERIOD_MS / 2;
            uint32_t ramp = phase < half ? phase : BREATHE_PERIOD_MS - phase;
            uint32_t lin = ramp * 255 / half;
            level = lin * lin / 255;
            break;
        }
        case LED_BLINK_FAST:
            level = (t % 250) < 125 ? 255 : 0;
            break;
        case LED_BLINK_SLOW:
            level = (t % 1000) < 500 ? 255 : 0;
            break;
        case LED_FLASH_2:
            level = flashLevel(t, 2);
            break;
        case LED_FLASH_3:
            level = flashLevel(t, 3);
            break;
    }
    write(level);
}

bool ledDone()
{
    return done;
}

void ledTestOrder()
{
    // LED_COLOR_ORDER_RGB sends the three values in argument order, so each
    // step drives exactly one byte on the wire.
    for (int i = 0; i < 3; i++)
    {
        uint8_t v[3] = {0, 0, 0};
        v[i] = LED_BRIGHTNESS;
        Serial.printf("[ledtest] byte %d on\n", i);
        rgbLedWriteOrdered(PIN_LED, LED_COLOR_ORDER_RGB, v[0], v[1], v[2]);
        delay(1500);
    }
    rgbLedWriteOrdered(PIN_LED, LED_COLOR_ORDER_RGB, 0, 0, 0);
    Serial.println("[ledtest] done; set LED_COLOR_ORDER to the colors seen, in byte order");
    // Make the next ledTick() repaint the current pattern.
    lastLevel = -1;
}
