#pragma once

#include <stdint.h>

#define BTN_DEBOUNCE_MS 20
#define BTN_LONG_PRESS_MS 1000

typedef enum
{
    BTN_NONE,
    BTN_CLICK,
    BTN_LONG_PRESS
} btn_event_t;

void btnInit();
// Call every loop. Reports one event per press, on release.
btn_event_t btnPoll();
