#include <Arduino.h>

#include <NimBLEDevice.h>
#include <SPI.h>

#include "acom.h"
#include "ble.h"
#include "button.h"
#include "led.h"
#include "packing.h"
#include "pins.h"
#include "storage.h"

// Same limits as the main firmware (fsm.h there).
#define PAIRING_TIMEOUT_MS 30000
#define BLE_CONNECT_TIMEOUT_MS 10000

#define BOOT_SHOW_MS 500
#define SUCCESS_SHOW_MS 2000

// Temporary: run the LED color-order test on every boot. Set to 0 once
// LED_COLOR_ORDER is right.
#define LED_BOOT_ORDER_TEST 0
#define LED_BOOT_ORDER_PAUSE_MS 3000

// Temporary: LED diagnosis on every boot. Solid red, green, blue (2 s each,
// dark gaps), then blue breathing for 6 s, all through the normal LED code.
#define LED_BOOT_DIAG 0

typedef enum
{
    STATE_BOOT,
    STATE_IDLE,
    STATE_PAIRING,
    STATE_BLE,
    STATE_RESULT,
    STATE_ERROR,
} state_t;

static state_t  state = STATE_BOOT;
static uint32_t stateEnterMs = 0;

static Contact self = {};

static volatile bool bleResultReady = false;
static volatile bool bleResultSuccess = false;
static uint8_t       bleRxBuf[PACKING_BUF_MAX];
static size_t        bleRxLen = 0;

static const char *stateName(state_t s)
{
    switch (s)
    {
        case STATE_BOOT:
            return "BOOT";
        case STATE_IDLE:
            return "IDLE";
        case STATE_PAIRING:
            return "PAIRING";
        case STATE_BLE:
            return "BLE";
        case STATE_RESULT:
            return "RESULT";
        case STATE_ERROR:
            return "ERROR";
    }
    return "?";
}

static void enterState(state_t next)
{
    Serial.printf("[state] %s -> %s\n", stateName(state), stateName(next));
    state = next;
    stateEnterMs = millis();

    switch (next)
    {
        case STATE_BOOT:
            ledSet(LED_WHITE, LED_SOLID);
            break;
        case STATE_IDLE:
            ledSet(LED_BLUE, LED_BREATHE);
            break;
        case STATE_PAIRING:
            ledSet(LED_CYAN, LED_BLINK_FAST);
            break;
        case STATE_BLE:
            ledSet(LED_PURPLE, LED_SOLID);
            break;
        case STATE_ERROR:
            ledSet(LED_RED, LED_BLINK_SLOW);
            break;
        case STATE_RESULT:
            // Color is set by the caller (success / already friends / failed).
            break;
    }
}

static uint32_t timeInState()
{
    return millis() - stateEnterMs;
}

static void showResult(led_color_t color, led_pattern_t pattern)
{
    enterState(STATE_RESULT);
    ledSet(color, pattern);
}

static void pairingFailed(const char *why)
{
    Serial.printf("[pair] failed: %s\n", why);
    showResult(LED_RED, LED_FLASH_3);
}

static void onBleComplete(bool success, const uint8_t *data, size_t len)
{
    if (success && data && len > 0 && len <= sizeof(bleRxBuf))
    {
        memcpy(bleRxBuf, data, len);
        bleRxLen = len;
    }
    else
    {
        success = false;
    }
    bleResultSuccess = success;
    bleResultReady = true;
}

static bool startBle()
{
    uint8_t ownMac[6];
    uint8_t peerMac[6];
    uint8_t peerType;

    acomGetOwnMac(ownMac);
    if (!acomHasMac(peerMac, &peerType))
    {
        return false;
    }

    // Same rule as the main firmware, so a rig and a full device agree on roles.
    ble_role_t role = (memcmp(ownMac, peerMac, 6) < 0) ? BLE_ROLE_SERVER : BLE_ROLE_CLIENT;
    Serial.printf("[ble] peer %02x:%02x:%02x:%02x:%02x:%02x type=%d role=%s\n", peerMac[0],
                   peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5], peerType,
                   role == BLE_ROLE_SERVER ? "SERVER" : "CLIENT");

    static uint8_t packBuf[PACKING_BUF_MAX];
    size_t         packLen = packingPack(packBuf, sizeof(packBuf));
    if (packLen == 0)
    {
        Serial.println("[ble] pack failed");
        return false;
    }

    bleResultReady = false;
    bleStart(peerMac, peerType, role, packBuf, packLen, onBleComplete);
    return true;
}

static void handleBleResult()
{
    bleResultReady = false;
    if (!bleResultSuccess)
    {
        pairingFailed("BLE transfer");
        return;
    }

    // A re-pair overwrites the same folder, so the count only grows for a new friend.
    int  before = storageCountContacts();
    char peerUuid[UUID_LEN];
    if (!packingUnpack(bleRxBuf, bleRxLen, peerUuid))
    {
        pairingFailed("unpack");
        return;
    }
    int after = storageCountContacts();

    if (after > before)
    {
        Serial.printf("[pair] new friend %s (total %d)\n", peerUuid, after);
        showResult(LED_GREEN, LED_SOLID);
    }
    else
    {
        Serial.printf("[pair] already friends with %s, profile updated\n", peerUuid);
        showResult(LED_YELLOW, LED_FLASH_2);
    }
}

static void printFriends()
{
    int count = storageCountContacts();
    Serial.printf("%d friend(s)\n", count);
    for (int i = 0; i < count; i++)
    {
        char name[NAME_LEN];
        if (storageLoadContactName(i, name, sizeof(name)))
        {
            Serial.printf("  %d: %s\n", i, name);
        }
    }
}

static void serialService()
{
    static char buf[32];
    static int  len = 0;

    while (Serial.available())
    {
        char c = Serial.read();
        if (c != '\n' && c != '\r')
        {
            if (len < (int)sizeof(buf) - 1)
            {
                buf[len++] = c;
            }
            continue;
        }
        if (len == 0)
        {
            continue;
        }
        buf[len] = '\0';
        len = 0;

        if (strcmp(buf, "list") == 0)
        {
            printFriends();
        }
        else if (strcmp(buf, "self") == 0)
        {
            Serial.printf("name=%s species=%s from=%s uuid=%s links=%d avatar=%d\n", self.name,
                           self.species, self.from, self.uuid, self.linkCount,
                           self.avatarResolution);
        }
        else if (strcmp(buf, "state") == 0)
        {
            Serial.printf("state=%s for %lu ms\n", stateName(state), timeInState());
        }
        else if (strcmp(buf, "ledtest") == 0)
        {
            ledTestOrder();
        }
        else
        {
            Serial.println("commands: list, self, state, ledtest");
        }
    }
}

#if LED_BOOT_DIAG
static void ledHold(const char *name, led_color_t color, led_pattern_t pattern, uint32_t ms)
{
    Serial.printf("[leddiag] %s\n", name);
    ledSet(color, pattern);
    uint32_t start = millis();
    while (millis() - start < ms)
    {
        ledTick();
        delay(1);
    }
}

static void ledBootDiag()
{
    ledHold("1 solid RED", LED_RED, LED_SOLID, 2000);
    ledHold("  off", LED_RED, LED_OFF, 1000);
    ledHold("2 solid GREEN", LED_GREEN, LED_SOLID, 2000);
    ledHold("  off", LED_RED, LED_OFF, 1000);
    ledHold("3 solid BLUE", LED_BLUE, LED_SOLID, 2000);
    ledHold("  off", LED_RED, LED_OFF, 1000);
    ledHold("4 BLUE breathing", LED_BLUE, LED_BREATHE, 6000);
    ledHold("  off", LED_RED, LED_OFF, 2000);
}
#endif

void setup()
{
    Serial.begin(115200);

    ledInit();
#if LED_BOOT_ORDER_TEST
    // Byte 0, 1, 2 lit alone in turn, then dark, so the LED's color order can be
    // read off without a serial terminal.
    ledTestOrder();
    delay(LED_BOOT_ORDER_PAUSE_MS);
#endif
#if LED_BOOT_DIAG
    ledBootDiag();
#endif
    enterState(STATE_BOOT);
    btnInit();

    NimBLEDevice::init("con-venience");
    NimBLEDevice::setMTU(517);
    Serial.printf("BLE MAC: %s\n", NimBLEDevice::getAddress().toString().c_str());

    acomInit();

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
    if (!storageInit())
    {
        Serial.println("[sd] no card or mount failed");
        enterState(STATE_ERROR);
        return;
    }

    char selfId[UUID_LEN];
    storageEnsureSelfId(selfId);
    if (!storageLoadSelf(self))
    {
        Serial.println("[sd] " SELF_JSON " / " SELF_BIN " missing or invalid");
        enterState(STATE_ERROR);
        return;
    }
    Serial.printf("self: %s (%s), %d friend(s)\n", self.name, self.uuid, storageCountContacts());
}

void loop()
{
    ledTick();
    serialService();
    btn_event_t btn = btnPoll();

    switch (state)
    {
        case STATE_BOOT:
            if (timeInState() >= BOOT_SHOW_MS)
            {
                enterState(STATE_IDLE);
            }
            break;

        case STATE_IDLE:
            if (btn != BTN_NONE)
            {
                acomStart();
                enterState(STATE_PAIRING);
            }
            break;

        case STATE_PAIRING:
        {
            if (btn != BTN_NONE)
            {
                acomStop();
                Serial.println("[pair] cancelled");
                enterState(STATE_IDLE);
                break;
            }

            acomTick();
            uint8_t peerMac[6];
            if (acomHasMac(peerMac, nullptr))
            {
                acomStop();
                enterState(STATE_BLE);
                if (!startBle())
                {
                    pairingFailed("BLE start");
                }
            }
            else if (timeInState() > PAIRING_TIMEOUT_MS)
            {
                acomStop();
                pairingFailed("no peer touched");
            }
            else if (acomFailed())
            {
                // One ACOM attempt timed out; keep probing until the pairing timeout.
                acomStart();
            }
            break;
        }

        case STATE_BLE:
            if (bleResultReady)
            {
                handleBleResult();
            }
            // Only the connect phase times out; once linked, the transfer ends on its own.
            else if (!bleIsConnected() && timeInState() > BLE_CONNECT_TIMEOUT_MS)
            {
                bleStop();
                pairingFailed("BLE connect timeout");
            }
            break;

        case STATE_RESULT:
        {
            bool shown = ledDone() || timeInState() >= SUCCESS_SHOW_MS;
            if (shown)
            {
                enterState(STATE_IDLE);
            }
            break;
        }

        case STATE_ERROR:
            // Fix the card on a PC and reset; the serial log says what is missing.
            break;
    }
}
