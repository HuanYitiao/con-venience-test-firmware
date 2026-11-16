#pragma once

#include <stdint.h>

typedef enum
{
    LED_OFF,
    LED_SOLID,
    LED_BREATHE,     // slow fade in/out
    LED_BLINK_FAST,  // 4 Hz, forever
    LED_BLINK_SLOW,  // 1 Hz, forever
    LED_FLASH_2,     // 2 blinks, then off
    LED_FLASH_3,     // 3 blinks, then off
} led_pattern_t;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

#define LED_WHITE {255, 255, 255}
#define LED_BLUE {0, 0, 255}
#define LED_CYAN {0, 255, 255}
#define LED_PURPLE {160, 0, 255}
#define LED_GREEN {0, 255, 0}
#define LED_RED {255, 0, 0}
#define LED_YELLOW {255, 160, 0}

// 0..255 cap applied to every color; the WS2812 is blinding at full power.
#define LED_BRIGHTNESS 32

// Byte order the LED expects on the wire (an rgb_led_color_order_t from the
// Arduino core). WS2812B is GRB; other parts differ. Run the `ledtest` serial
// command to find out which channel each byte drives.
#ifndef LED_COLOR_ORDER
#define LED_COLOR_ORDER LED_COLOR_ORDER_GRB
#endif

void ledInit();
void ledSet(led_color_t color, led_pattern_t pattern);
// Call every loop; never blocks.
void ledTick();
// True once a finite pattern (LED_FLASH_*) has played out.
bool ledDone();
// Blocking: lights wire byte 0, 1, 2 alone for 1.5 s each, logging which is on.
void ledTestOrder();
