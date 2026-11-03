# LaunchLab BLE protocol, version 1

Peripheral name: `LaunchLab-XXXX`. All integers and floats are little endian.
Bluetooth is off until enabled on the device, under Menu, Bluetooth; changing it saves and restarts the stick.

| Characteristic | UUID | Use |
| --- | --- | --- |
| Service | `4c4c0001-6265-7977-6174-63686c61626c` | |
| Events | `4c4c0002-6265-7977-6174-63686c61626c` | notify |
| Command | `4c4c0003-6265-7977-6174-63686c61626c` | write with response |
| Reply | `4c4c0004-6265-7977-6174-63686c61626c` | notify, framed |

## Events

Launch, 13 bytes, sent once when a pull has ended:
`type u8 = 1, number u32, session u32, rpm f32`.

`number` only rises on one device. Ignore any other `type`. No launch events are sent in tournament mode.
The curve for this pull can be requested as soon as the event arrives; a later pull replaces it.

## Pairing

All characteristics require an encrypted, bonded link: LE Secure Connections with Numeric Comparison, MITM protected. A phone without a bond can ask to pair at any time: the stick shows the 6-digit number over its screen and the phone's system dialog shows the same one. With no bond on the stick yet, it accepts by itself after 4 s, B rejecting until then, and the phone's dialog still asks; with at least one bond, A on the stick confirms, B rejects, and 30 s without an answer rejects. A peer that cannot do Numeric Comparison (no display with yes/no, or Just Works) is refused. Bonded phones connect whenever Bluetooth is on, with no prompt. The stick keeps its bonds across restarts and offers Forget phones on its Bluetooth page under Menu, which also ends the current link and returns it to having no bond.

The stick refuses a request, without queueing it, during a pull or its capture, in tournament mode, or while a setting editor, Wipe or another pairing is open. After 3 rejected or timed-out requests it refuses new ones for 60 s. A phone that is refused or fails pairing gets disconnected without any data, as does a link not secured within 60 s. Subscriptions do not survive the connection: subscribe again each time.

## Commands and replies

Write `op u8` followed by its arguments. One command runs at a time: wait for the last frame before the next.

Every reply frame is `op u8, seq u8, flags u8, payload...`. `seq` starts at 0 and wraps. Flag bit 0 marks the last frame; bit 1 marks an error, whose payload is one status byte. Concatenate the payloads in order. A reply may be delayed until a pull has ended, so allow 15 s.

Status: `0` ok, `1` invalid value, `2` busy (pull, menu editor or wipe open on the device, try again), `3` hidden in tournament mode, `4` unknown command.

Set setting and New session report status in their normal payload; other commands use an error frame, which also has the last-frame flag. In tournament mode Get launches, Get sessions and Get curve return status 3, and Set setting and New session return 2. Nothing announces entering or leaving tournament mode: poll Get info. There is no request id: after a timeout, ignore frames until the next command's first frame with `seq` 0.

The service UUID is in the advertisement and the name in the scan response. Get sessions includes the open session. Get curve before any pull since boot returns `count` 0, and curves do not survive a restart.

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 1 | Get info | none | see below |
| 2 | Get settings | none | repeated `id u8, value u8` |
| 3 | Set setting | `id u8, value u8` | `status u8` |
| 4 | Get status | none | see below |
| 5 | Get launches | `fromNumber u32` | repeated launch, oldest first, `number >= fromNumber` |
| 6 | Get sessions | none | repeated session, oldest first |
| 7 | Get options | `id u8` | repeated `value u8, length u8, label` (UTF-8) |
| 8 | Get curve | none | `number u32, peakRpm u16, count u16, leadMs u16`, then repeated `ms u16, rpm u16` |
| 9 | New session | none | `status u8` |
| 10 | Get power log | none | repeated power record, oldest first |
| 13 | Set name | `length u8, name` (1-16 bytes of ASCII letters, digits, `-` or `_`) | `status u8` |

**Curve:** `leadMs` is how far `ms` 0 lies before the pull's optical start: the slow turns that led into it, up to three (0.6 s). It is 0 when there were none, and before any pull.

**Power record:** 13 bytes, `trackedSeconds u32, boot u32, batteryMillivolts u16, event u8 (1 boot, 2 sample, 3 sleep, 4 wake), flags u8, brightness u8`. Flag bits: 1 charging, 2 charge state known, 4 LED, 8 boost, 16 tournament, 32 sensor rail. The device keeps the last 48 records, one sample a minute while awake, also when no phone is connected. `trackedSeconds` counts awake and known sleep time across boots.

A firmware older than this protocol revision answers op 10 with status 4: treat that as "not supported".

## Tilt replay

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 11 | Get replay | none | `number u32, durationMs u32, count u16, flags u8`, then repeated point |

**Point:** 12 bytes, `ms i32, w i16, x i16, y i16, z i16`. The quaternion is scaled by 16384 and is the launcher's pose at `ms`. `ms` 0 is the pull's optical start, where the three turns confirming it begin, and `durationMs` runs from there. Points before it are the lead-in. A curve point's time on this clock is `ms - leadMs`, using Get curve's `leadMs`. `count` is 48, or 0 when the device could not draw this pull's motion (too little data, or a gap in it). Flag bit 0 means approximate: a sensor reached its limit or a short gap was bridged, as on hard pulls; the points are sent and the device draws them marked Tilt approx.

Status 2 means the capture of the latest pull is still finishing: retry after 300 ms, for up to 3 s. Status 3 in tournament mode. It replaces on the next pull and does not survive a restart, so fetch it right after the launch event.

**Frame:** launcher axes, whatever the stick's Mounting, the same as a Top-mounted stick's: +X along the barrel, +Y toward the ripcord handle, -Y toward the cord, +Z up, with real gravity-relative roll and pitch kept. Compass heading is meaningless: each replay turns it so the barrel (+X) starts at 45° in the horizontal plane. Position, height and speed are not measured, so the model stays centred.

**View:** from above and to one side: barrel toward the viewer and to the left, ripcord handle to the right. Turn each posed point `(x, y, z)` by -45° about Z, which undoes the replay's heading: `(u, v) = ((x + y) * 0.7071, (y - x) * 0.7071)`. Then project with `sx = v * 0.866 - u * 0.5`, `sy = (u * 0.866 + v * 0.5) * 0.643 - z * 0.766` (camera 40° above the floor, 30° to the side). The direction toward the viewer is `(0.663, 0.383, 0.643)` in `(u, v, z)`. The device's own screen is drawn true to the glass, each part pointing where it really is from the screen as held: this view turned about the vertical so the end at the top of the screen is the far end, and by 30° more to take out the side angle. With Flip Off, Top 90 R has the barrel straight down and the ripcord handle to the right, Top 90 L the barrel up and the handle left, Top and Sidecar the barrel left and the handle down; Flip On turns each by a further 180°. An app may start from the same height but straight on, with no side angle (`sx = v`, `sy = u * 0.643 - z * 0.766`).

**Floor:** a level ring stays fixed under the model as the reference it tilts against: radius 0.95 at z -0.32, in `(u, v, z)`, not turned by the pose.

## Level reference

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 14 | Set level reference | `action u8`: `0` clear, `1` set now | `status u8` |

Set stores the current filtered up direction as level, so a launcher resting flat reads 0° everywhere: the level reading, Get status's `levelDegrees`, live tilt events, replay points and the model on screen. It does not touch the IMU, the fusion or RPM. Status 2 while the launcher is moving, the reading is not valid, a pull is being measured or captured, in tournament mode or while an editor is open. Status 1 for another `action`, or when the correction would exceed 20°, which means the wrong Mounting. It persists across restarts, is not in backups, and changing Mounting clears it. `levelSet` in Get info is 1 while one is set.

## Live tilt

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 12 | Set live tilt | `enabled u8` | `status u8` |

While enabled and connected, the device sends an event about 10 times a second: 9 bytes, `type u8 = 2, w i16, x i16, y i16, z i16`. It is the launcher's latest fused pose (the same fusion as the replay, gyro and accelerometer), scaled by 16384. Roll and pitch are true to gravity. Yaw comes from the gyro alone: it follows real turns but starts anywhere and drifts slowly, since there is no compass. Nothing is sent while a pull is being measured or when the fusion is not ready, and streaming stops on disconnect. Enable it only while the live view is on screen.

To draw it, remove the pose's yaw offset with one heading turn about Z, so the barrel (+X) points at 45° in the horizontal plane at the moment of reference, then the View below applies unchanged. Take the reference from the first valid event, and let the user set it again. It stays valid while the link lasts, since the fusion keeps running with the stream off. A replay resets its own heading, so to show a pull the way the live view showed the launcher, turn the replay by the live view's heading from 45° just before the pull.

## Stick display

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 15 | Set display | `on u8` | `status u8` |

0 turns the stick's backlight and drawing off for this connection, for a phone that shows the readings itself. The measurement is unchanged. A button press lights the screen for 10 s and does nothing else, the numeric comparison always shows, and the screen returns when the link ends. It is not saved, so send it again after each connection. An older device answers unknown command.

## Sensor diagnostics

| op | Name | Arguments | Reply payload |
| --- | --- | --- | --- |
| 16 | Get diagnostics | none | see below |

**Diagnostics:** 39 bytes, read only: `state u8 (as Get status), minMark u16, lastMark u16, medianMark u16, noise u16, signalMin u16, signalMax u16, samplesPerSecond u16, weak u32, slow u32, shape u32, inconsistent u32, dataLoss u32, maxReadGapUs u32`.

Marks and signal values are ADC counts. `minMark` is the current Sensor profile's minimum, below which a mark does not count. `lastMark` is the weakest of the three start marks of the latest confirmed pull since boot, `medianMark` the median over the last few pulls, which the stick keeps across restarts until the Sensor profile changes; each is 0 with none yet. `noise` is the detector's noise band and `signalMin`, `signalMax` the recent range. A shaft resting where little light returns reads 4095, which is not a fault. `samplesPerSecond` is 0 at rest, when optical sampling pauses (`state` 2).

The counters run since boot. `weak`, `slow`, `shape` and `inconsistent` are movement the detector looked at and did not take for a pull; they rise in normal use, so read a change rather than the size. `dataLoss` adds up ADC overflows, read errors, invalid frames, lost events and stalls. `maxReadGapUs` is the longest gap between ADC reads.

Status 3 in tournament mode. An older device answers unknown command. Ask no more than about once a second, and only while it is on screen.

## Launcher model

The device draws this simple shape, in launcher axes, as flat-shaded boxes; the app may draw more detail within about the same extents. Subtract the centre (0.6, -0.1, 0), then multiply by 0.62, so the turn stays on screen.

| Part | Colour | Extent |
| --- | --- | --- |
| Head | dark | x -0.45..0.45, y -0.45..0.45, z -0.18..0.15 |
| Neck | launcher | x 0.45..0.85, y -0.36..0.36, z -0.14..0.10 |
| Barrel | launcher | x 0.85..2.10, y -0.30..0.30, z -0.16..0.08 |
| Ripcord stem | launcher | x -0.05..0.05, y 0.45..0.78, z -0.05..0.05 |
| Ripcord bar | launcher | x -0.22..0.22, y 0.78..0.86, z -0.05..0.05 |
| Cord | launcher | x -0.02..0.02, y -1.10..-0.45, z -0.02..0.02 |

The stick follows Mounting. Its body is light, with a dark screen always drawn over it, nearer its pin end; its USB end is the other end:

| Mounting | Stick body | Screen | Seen in the view |
| --- | --- | --- | --- |
| Top | x -0.20..0.20, y -0.38..0.38, z 0.15..0.36 | x -0.13..0.13, y -0.10..0.28, z 0.36..0.37 | across the head, USB left, pins to the ripcord handle |
| Top 90 R | x -0.38..0.38, y 0.03..0.43, z 0.15..0.36 | x -0.10..0.28, y 0.10..0.36, z 0.36..0.37 | along the barrel on the head's right half, USB away from the barrel |
| Top 90 L | x -0.38..0.38, y -0.43..-0.03, z 0.15..0.36 | x -0.28..0.10, y -0.36..-0.10, z 0.36..0.37 | along the barrel on the head's left half, USB toward the barrel |
| Sidecar | x -0.66..-0.45, y -0.38..0.38, z -0.20..0.20 | x -0.67..-0.66, y -0.10..0.28, z -0.13..0.13 | upright at the head's far side, screen away from the barrel |

It is a schematic after the common string launcher, not an exact shape.

## Mounting

Setting 9 has four values, with labels from Get options: `0` Top, `1` Sidecar, `2` Top 90 L, `3` Top 90 R. Top has the stick across the head, USB toward the cord and pins toward the ripcord handle. The two rotated values are Top turned 90° in the plane of the launcher's top, counter-clockwise and clockwise seen from above, so the stick runs along the barrel: Top 90 R has its USB end pointing away from the barrel, Top 90 L toward it: the replay and tilt then read in launcher axes, while the stick's screen stays in portrait relative to the stick.

**Info:** `protocol u8 = 1, lastNumber u32, session u32, launches u16, sessions u16, tournament u8, versionLength u8, version, nameLength u8, name, levelSet u8`.

**Name:** the default is `LaunchLab-XXXX` from the Bluetooth address. A set name is advertised as `LaunchLab-<name>`, persists across restarts, and applies when the stick next advertises. Get info's `name` is the name as advertised. Set name returns status 1 for an invalid name and 2 in tournament mode or while an editor is open.

**Status:** `batteryPercent i8 (-1 unknown), batteryMillivolts u16, charging u8 (0 no, 1 yes, 2 unknown), state u8 (0 ready, 1 pull, 2 resting, 3 sensor fault), levelDegrees f32 (NaN unknown), sleepInSeconds u32 (0 awake while connected)`.

**Launch:** 16 bytes, `number u32, session u32, rpm f32, sessionSeconds u32`.

**Session:** 20 bytes, `number u32, pulls u32, averageRpm f32, bestRpm f32, seconds u32`.

The device keeps 128 launches and 24 sessions; older ones roll off. It has no clock: stamp a live launch with the phone's time. For fetched launches, order them by `number`, since `sessionSeconds` only counts within a session.

## Settings

Show the labels from Get options rather than hard-coding them. Values apply at once and are saved by the device.

| id | Setting |
| --- | --- |
| 0 | Sleep after |
| 1 | RPM method (starts a new session) |
| 2 | Brightness |
| 3 | Flip display |
| 4 | Sensor profile (starts a new session) |
| 5 | Theme |
| 6 | Best effect |
| 7 | Celebrate |
| 8 | Sound |
| 9 | Mounting |
| 10 | Shake to wake |

Get settings also returns id 11, Bluetooth. Set setting accepts it only with value 0: the reply comes first with status 0, then the stick saves and restarts with its radio off, so the link ends; status 2 during a pull. Turning it on takes the stick, its radio being off then; value 1 returns status 1. Wipe, restore and power off are deliberately not available over BLE.

## Behaviour while connected

The device does not sleep while a phone is connected. It still pauses optical sampling when at rest. Disconnect when the phone leaves the LaunchLab screen for long, to save the stick's battery.
