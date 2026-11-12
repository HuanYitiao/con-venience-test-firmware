# con-venience-test-firmware

![Platform](https://img.shields.io/badge/platform-ESP32--c6-blue)
![Framework](https://img.shields.io/badge/framework-Arduino%20%7C%20PlatformIO-orange)

A minimal test rig for the pairing flow of [con-venience-firmware](https://github.com/HuanYitiao/con-venience-firmware).

It runs only the pairing path: ACOM contact handshake, BLE profile exchange, and saving the result to the SD card. The display, audio, RTC and GPIO expander are left out, so pairing problems can be reproduced without the rest of the system getting in the way.

## Use cases

- Stress-test ACOM and BLE pairing (success rate, timeouts, CRC errors)
- Act as a pairing partner when testing a single full device
- Check that protocol changes in the main firmware stay compatible

## Hardware

- ESP32-C6-DevKitC-1 (uses the onboard WS2812 RGB LED for status)
- One push button
- MicroSD card module (SPI)
- ACOM contact interface (same circuit as the full device)

## Pins

| Function   | GPIO |
|------------|------|
| SPI SCK    | 11   |
| SPI MISO   | 10   |
| SPI MOSI   | 2    |
| SD CS      | 3    |
| ACOM OD    | 19   |
| ACOM PU    | 18   |
| RGB LED    | 8    |
| Button     | TBD  |

## Test flow

1. Press the button to start pairing.
2. Touch the ACOM contacts of two devices. They exchange BLE MAC addresses over a 4800-baud single-wire UART.
3. The devices connect over BLE and exchange profiles.
4. The peer's profile is saved to the SD card.

The ACOM frame format and the BLE profile protocol match the main firmware, so the rig can pair with either another rig or a full device.

## LED status

| State                       | Color  | Pattern              |
|-----------------------------|--------|----------------------|
| Booting                     | White  | Solid, ~0.5 s        |
| Idle                        | Blue   | Slow breathing / off |
| ACOM probing                | Cyan   | Fast blink           |
| BLE exchanging              | Purple | Solid                |
| Success                     | Green  | Solid 2 s            |
| Pairing failed              | Red    | 3 fast blinks        |
| Already friends             | Yellow | 2 blinks             |
| No SD card / no profile     | Red    | Slow blink (locked)  |

## SD card layout

```
/self_profile/profile.json    test profile sent by this rig
/self_profile/avatar.bin      test avatar (optional)
/friends_profiles/            profiles received from peers
```

The layout matches the main firmware. Put a test profile on the card before use:

```json
{
  "name": "Test Rig A",
  "species": "Test",
  "from": "Bench",
  "links": [
    { "platform": "platform", "url": "https://platform/test" }
  ],
  "avatar_res": 128,
  "avatar_mode": 1
}
```

## Build

Requires PlatformIO. Open this repository as the workspace root in VS Code, then build and upload to the `esp32-c6-devkitc-1` environment.

## License

MIT
