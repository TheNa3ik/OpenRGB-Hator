# Hator Pulsar 3 Pro driver for OpenRGB

This folder adds support for the **Hator Pulsar 3 Pro** wireless mouse to OpenRGB. It lives in a fork of [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) and is not part of the official project. It is independent community work, not affiliated with Hator or with the OpenRGB project.

## Status

Tested by the author on **Windows 11** with one mouse, over both the 2.4 GHz dongle and the USB cable. Linux and macOS are untested.

OpenRGB sends reverse-engineered commands to hardware. The commands used here were decoded from USB captures, but there is always some risk when sending data to a device whose firmware is not documented. Use it at your own risk.

## Supported hardware

| Connection | VID | PID |
|---|---|---|
| 2.4 GHz wireless dongle | `0x379A` | `0x3904` |
| USB cable | `0x379A` | `0x3903` |

The mouse appears in OpenRGB as one device with two zones, **Underglow** and **Wheel**.

## What works

**Underglow modes** (same names and order as the Hator software): Off, Breathing, Static, Neon, Rainbow Breathing, Rainbow Wave, Colorful. Color, speed and brightness are available where the mode supports them.

**Wheel modes:** Off, Fixed (color and brightness) and Breathing (color and speed). If the Wheel zone is set to "Follow Device Mode", the device-level Off, Static and Breathing modes are applied to the wheel as well.

## Known limitations

- **Charging:** while the USB cable is charging the mouse, the mouse shows its own battery indicator on the underglow and ignores underglow commands. The wheel is not affected.
- **No read-back:** the driver cannot read the mouse's current settings, so the modes shown in OpenRGB at startup are only a starting point.
- **Wheel color:** the wheel shows the color of the active DPI profile, and the mouse cannot report which profile is active. You choose the profile in the configuration below.
- **Dongle and cable:** if the cable is plugged in while OpenRGB is running, the dongle entry is removed so the mouse is not listed twice. After unplugging the cable, press **Rescan** to bring the dongle entry back.
- **Windows only so far:** on Windows each command is sent as a HID output report with `HidD_SetOutputReport`, because `hid_write` fails on this device. Other platforms use `hid_write` and are untested.

## Configuration

The driver reads an optional `HatorPulsar3Pro` section from `OpenRGB.json`, which is in `%APPDATA%\OpenRGB`. Close OpenRGB before editing it. Add the section at the top level of the file, with a comma after it if another section follows.

```json
"HatorPulsar3Pro": {
    "idle_timeout_seconds": 60,
    "lights_off_while_moving": true,
    "wheel_dpi_profile": 2
},
```

Every key is optional. Any key you set is sent to the mouse each time OpenRGB starts; keys you leave out are never sent, because the mouse keeps its own settings.

| Key | Values | Meaning |
|---|---|---|
| `wheel_dpi_profile` | 1 to 4 | The DPI profile whose color the wheel shows. Profiles 5 and 6 exist in the code but are untested. |
| `lights_off_while_moving` | `true` or `false` | Turn the lights off while the mouse is moving. |
| `idle_timeout_seconds` | 10, 30, 60, then every 5 minutes: 300, 600, 900, 1200, 1500, 1800, 2100, 2400 | Turn the lights off after this much idle time.  |

## Protocol notes

Decoded from USB captures of the official Hator software. No proprietary code from Hator is included.

Every command is one 17-byte HID output report with report ID `0x08`:

| Byte | Meaning |
|---|---|
| 0 | `0x08` report ID |
| 1 | `0x07` write setting |
| 2-3 | `0x00 0x00` |
| 4 | setting address |
| 5 | payload length (data bytes + 1) |
| 6.. | data bytes, then an inner checksum `0x55 - sum(data)` |
| rest | zero padding |
| 16 | frame checksum `0x55 - sum(bytes 0..15)` |

Setting addresses:

| Address | Data |
|---|---|
| `0xA0` | Underglow, 6 bytes: effect, R, G, B, speed (0-9), brightness (0-9) |
| `0xA7` | Underglow on/off (1 or 0) |
| `0xB3` | Lights off while moving (0 or 1) |
| `0xAD` | Idle timer, in units of 10 s |
| `0x04` | Active DPI profile (0-based) |
| `0x2C + 4*n` | Color of DPI profile n (R, G, B). Captured for n = 0 to 3 |
| `0x52` | Wheel on/off |
| `0x4C` | Wheel mode (1 fixed, 2 breathing) |
| `0x50` | Wheel breathing speed (1 to 5) |
| `0x4E` | Wheel brightness (`0x10` to `0xFF`) |

Underglow effect codes: `01` Rainbow Wave, `02` Breathing, `03` Static, `04` Neon, `05` Rainbow Breathing, `06` Colorful.

The idle timer value is the number of seconds divided by 10, for example 1 min = `06`, 5 min = `1E`, 40 min = `F0`.

To color a DPI profile, the official software first sends `0x04 = n` and then the color, which also makes profile n the active one. The driver does the same.

## License

Same as OpenRGB: GPL-2.0-or-later. See the `LICENSE` file in the repository root.