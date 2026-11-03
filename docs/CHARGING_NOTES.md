# Charging notes: StickS3 on USB

Bench notes from one M5StickS3 K150, measured on 2026-11-02 and 2026-11-03. They cover one unit and a few runs, so treat them as observations and not as a characterisation of the hardware.

## Summary

While the firmware runs on USB, the battery stopped rising near 3.96 V in four of the five runs and near 3.92 V in the fifth, which the display shows as about 82 to 90%. Stock 0.20.5 reached the same 3.96 V level as the BLE prototype, so this does not look like a BLE regression, with one unexplained run noted below. Left powered off on USB overnight, the same unit read 4.10 to 4.12 V and 100%.

The likely cause is that the running firmware draws about as much as the charger supplies, which leaves little for the cell. This is an inference: the firmware cannot measure current, and no external meter was used.

## Measurements

Each run lasted 20 minutes on a PC USB port, with the stick running and idle. `charge_trace` read the `p` status every 5 s. Values are medians of the first and last third of the samples.

| Firmware | Bluetooth | Flag low (mV) | Flag high (mV) | Samples with flag high |
|---|---|---|---|---|
| Stock 0.20.5 | n/a | 3906 → 3958 | 3964 → 4024 | 64% |
| Stock 0.20.5, started near 3.97 V | n/a | 3962 → 3962 | 4026 → 4025 | 35% |
| BLE prototype | Off, after restart | 3964 → 3962 | 4026 → 4031 | 39% |
| BLE prototype | On, no phone | 3984 → 3964 | 4026 → 4012 | 45% |
| Earlier BLE prototype build | On, no phone | 3924 → 3926 | 3972 → 3974 | 51% |

"Flag" is the `charging` value from PM1 G0, active low.

Powered off overnight on USB, BLE prototype: 4102 to 4118 mV and 100% during the first minute or so after power-on, with `charging=not_charging`. The exact charge time was not recorded, and neither was the voltage at which the charger stopped.

## What the code does

- **Percentage:** `StickS3Battery::update` in `power_status.h` maps voltage linearly, 3300 mV to 0% and 4100 mV to 100%, from a median of five samples. Its comment already calls this an approximate estimate. The file is unchanged from stock.
- **Charging control:** neither firmware writes a charge setting. `pm1_cfg` read `0x07` on both, so `M5PM1_PWR_CFG_CHG_EN` (bit 0) is set. The M5Unified driver says PM1 has no charge current or charge voltage register.
- **Staying awake:** `UsbHostAwake::inhibited` in `usb_awake.h` blocks auto-off while a USB host is present, or while VIN stays present after a host was seen. `IdlePowerPolicy::brightness` also skips the idle dim in that state. So a stick on a PC keeps running. A charger with no host should still reach auto-off, but this was not tested.

## Observations worth a second look

- **Bimodal voltage:** with the battery updated every 5 s, the prototype stepped between about 3.97 V and 4.02 V from one update to the next. A one-minute stock trace, taken later and at a different moment, rose steadily from 4018 to 4036 mV instead. Both traces are short, so this does not show a difference between the firmwares.
- **Flag and voltage disagree:** readings of 4020 mV with `not_charging` and 3970 mV with `charging` both occur. So the "flag low" column is not a clean battery-only voltage, and the charger may be switching faster than the 5 s sampling can show.
- **Percentage jumps:** the median resets when the flag changes, so the display can move by several points on USB.

## Open questions

- **The 3.92 V run:** the earlier prototype build stayed flat at 3924 mV, while stock rose from 3906 to 3958 mV over a similar start. That build and the later ones differ, and the battery state differed too, so a BLE effect on charging at lower voltages is not ruled out. A repeat from about 3.90 V on both firmwares would settle it.
- **Current:** an inline USB meter would show the actual draw and test the explanation directly.
- **Stock over a long run:** neither firmware was charged running for longer than 20 minutes, so a slow rise would not have shown.
- **Plain charger:** auto-off and charging on a USB charger with no host.

## Ideas

These are display changes that do not touch acquisition:

- On USB, show "Charging" or "USB" instead of a percentage, since the voltage there says little about the cell.
- Replace the linear map with a small lithium-ion voltage table for readings on battery only.
- Smooth the charging flag over a few samples, so it stops flickering.
