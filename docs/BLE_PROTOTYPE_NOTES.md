# Bluetooth LE prototype: what was measured

Bench notes for reviewing this branch. It is a prototype on top of 0.20.5, not a release. All measurements are from one M5StickS3 K150, on 2026-11-01 to 2026-11-03, with few runs per state, so treat them as a first look. The project's hardware-in-the-loop suite is not public and has not been run.

## What the branch adds

- **Bluetooth:** a BLE service with numeric comparison pairing, off by default, enabled under Menu › Bluetooth. Contract: [BLE_PROTOCOL.md](BLE_PROTOCOL.md).
- **Stick display switch:** a phone can turn off the stick's backlight and drawing for the length of a connection.
- **Outside Bluetooth, which can be split off:** Top 90 L and Top 90 R mountings, Set as flat for the level, a Live tilt page, a more detailed launcher model in the replay, a recap that keeps the direction the launcher was held in, and a ripcord that leaves during the recap.

## Performance

| | Stock 0.20.5 | This branch |
|---|---|---|
| Free internal RAM | about 54.7 KB | about 71 KB with Bluetooth, about 75 KB off |
| Smallest free DMA block | 32 bytes | about 55 KB |
| Max read gap during pulls | 20.5 ms | 21.8 and 22.1 ms |
| Max read gap at a history save | 36.7 ms | 26.0 and 27.1 ms, and 34.3 ms once |

- **Memory:** the canvas and `LaunchMotion::Ring` moved to PSRAM to make room. With Bluetooth off at boot the canvas stays internal. The ring copy took about 3 ms at most.
- **Read gap:** each figure is the maximum since a restart, over 5 to 10 real pulls on USB. Stock is one run of 6 pulls. The 34.3 ms came from an interrupted run without clear timing. Saves vary a lot between runs, so the save row does not show this branch to be better. The difference during pulls is about 1.3 to 1.6 ms.
- **Accuracy:** 11 of 11 pulls reported identical values over USB and BLE at the same time.
- **Screen off:** it made no measurable difference to the read gap. Max draw time was 25.7 ms against 39.0 ms with the screen on.

## Battery

| Run | Drop | Length |
|---|---|---|
| Bluetooth off, an earlier build of this branch | 259 mV per hour, 3918 → 3802 mV | 27 min |
| Phone connected, current build | 173 mV per hour, 3902 → 3776 mV | 42 min |

These two runs **cannot be compared**, since they differ in build and length. Connected looks better than off, which is not believable as a Bluetooth effect. There is no stock run yet. Voltage is only a rough proxy for charge. A connected phone blocks auto-off, so a stick stays awake as long as a phone stays connected.

Charging has its own notes: [CHARGING_NOTES.md](CHARGING_NOTES.md).

## Tests

`scripts/test-firmware.sh` passes. `shake_wake` needs clang and ran on its own with `CXX=clang++`, the rest with GCC. The test `dark_display` is new. Some checks that compile parts of `loop()` gained stubs for the new helpers.

## Known gaps

- A spoofed address can erase one bond.
- The first phone can be accepted automatically in a 4 s window.
- No comparison battery runs yet: off, on, connected and stock, on one build and from a similar voltage.
