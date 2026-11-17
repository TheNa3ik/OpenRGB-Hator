/*---------------------------------------------------------*\
| HatorPulsar3ProController.h                               |
|                                                           |
|   Driver for Hator Pulsar 3 Pro wireless mouse            |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <hidapi.h>
#include "RGBController.h"

#define HATOR_VID                       0x379A
#define HATOR_PULSAR_3_PRO_PID          0x3904  /* wireless dongle */
#define HATOR_PULSAR_3_PRO_USB_PID      0x3903  /* USB cable       */

/*---------------------------------------------------------*\
| Protocol (decoded from USB captures)                      |
|                                                           |
| Every command is one 17-byte Output report (ID 0x08):     |
|                                                           |
|   [0]    0x08  report ID                                  |
|   [1]    0x07  "write setting" command                    |
|   [2-3]  0x00 0x00                                        |
|   [4]    setting address                                  |
|   [5]    payload length (data bytes + 1 checksum byte)    |
|   [6..]  data bytes, then inner checksum                  |
|          inner checksum = 0x55 - sum(data bytes)          |
|   ...    zero padding                                     |
|   [16]   frame checksum = 0x55 - sum(bytes 0..15)         |
|                                                           |
| Known setting addresses:                                  |
|   0xA0  underglow lighting (6 data bytes):                |
|           effect, R, G, B, speed (0-9), brightness (0-9)  |
|   0xA7  underglow on/off (1 data byte, 0 or 1)            |
|   0xB3  lights off while moving (0 or 1)                  |
|   0xAD  idle lights-off timer (1 data byte, units of 10 s)|
|   0x04  active DPI profile index (0..)                    |
|   0x2C + 4*n  DPI profile n color (R, G, B)               |
|   0x52  wheel backlight on/off (0 or 1)                   |
|   0x4C  wheel backlight mode (1 = fixed, 2 = breathing)   |
|   0x50  wheel breathing speed (1..5)                      |
|   0x4E  wheel brightness (0x10..0xFF)                     |
|                                                           |
| The wheel shows the color of the active DPI profile. The  |
| mouse cannot tell us which profile is active, so the      |
| profile to color is configured by the user (0-based).     |
| Writing a profile's color requires selecting it first     |
| (0x04), which also makes it the active profile.           |
|                                                           |
| Note: while the USB cable is charging the mouse, it shows |
| its own battery indicator on the underglow and ignores    |
| underglow commands. The wheel is not affected.            |
\*---------------------------------------------------------*/

#define HATOR_PACKET_SIZE               17
#define HATOR_REPORT_ID                 0x08
#define HATOR_CMD_WRITE                 0x07
#define HATOR_CHECKSUM_BASE             0x55

#define HATOR_ADDR_MAIN_LIGHT           0xA0
#define HATOR_ADDR_MAIN_LIGHT_ENABLE    0xA7
#define HATOR_ADDR_LIGHTS_OFF_MOVING    0xB3
#define HATOR_ADDR_IDLE_TIMEOUT         0xAD

#define HATOR_ADDR_DPI_PROFILE          0x04
#define HATOR_ADDR_DPI_COLOR_BASE       0x2C
#define HATOR_DPI_COLOR_STRIDE          4
#define HATOR_ADDR_WHEEL_MODE           0x4C
#define HATOR_ADDR_WHEEL_BRIGHTNESS     0x4E
#define HATOR_ADDR_WHEEL_SPEED          0x50
#define HATOR_ADDR_WHEEL_ENABLE         0x52

/* DPI profile used for the wheel color unless configured otherwise (0-based) */
#define HATOR_DEFAULT_WHEEL_PROFILE     1
#define HATOR_MAX_WHEEL_PROFILE         5

/*---------------------------------------------------------*\
| Timing, in milliseconds                                   |
|   FRAME_DELAY: pause after every frame                    |
|   PROFILE_SWITCH_DELAY: pause between selecting a DPI     |
|   profile and writing its color                           |
\*---------------------------------------------------------*/
#define HATOR_FRAME_DELAY_MS            100
#define HATOR_PROFILE_SWITCH_DELAY_MS   250

/*---------------------------------------------------------*\
| Effect byte sent to address 0xA0                          |
\*---------------------------------------------------------*/
enum
{
    HATOR_EFFECT_RAINBOW_WAVE           = 0x01,
    HATOR_EFFECT_BREATHING              = 0x02,
    HATOR_EFFECT_STATIC                 = 0x03,
    HATOR_EFFECT_NEON                   = 0x04,
    HATOR_EFFECT_RAINBOW_BREATHING      = 0x05,
    HATOR_EFFECT_COLORFUL               = 0x06,
};

/*---------------------------------------------------------*\
| Wheel mode byte sent to address 0x4C                      |
\*---------------------------------------------------------*/
enum
{
    HATOR_WHEEL_FIXED                   = 0x01,
    HATOR_WHEEL_BREATHING               = 0x02,
};

#define HATOR_WHEEL_SPEED_MAX           5

/* Internal marker for the "Off" modes, never sent to the mouse */
#define HATOR_MODE_OFF                  0xFF

class HatorPulsar3ProController
{
public:
    HatorPulsar3ProController(hid_device* dev_handle, const char* path, std::string dev_name);
    ~HatorPulsar3ProController();

    std::string GetName();
    std::string GetLocation();
    std::string GetSerial();

    /*-----------------------------------------------------*\
    | Underglow lighting                                    |
    |   speed and brightness are 1..10 (clamped inside)     |
    \*-----------------------------------------------------*/
    void        SetMainLightEnabled(bool enabled);
    void        SetMainEffect(uint8_t effect, uint8_t red, uint8_t grn, uint8_t blu,
                       unsigned int speed, unsigned int brightness);

    /*-----------------------------------------------------*\
    | Wheel (DPI) lighting                                  |
    |   brightness is 1..10, speed is 1..5 (clamped inside) |
    |   profile is 0-based (0..HATOR_MAX_WHEEL_PROFILE)     |
    \*-----------------------------------------------------*/
    void        SetWheelProfile(unsigned int profile);
    void        SetWheelEnabled(bool enabled);
    void        SetWheelColor(uint8_t red, uint8_t grn, uint8_t blu);
    void        SetWheelMode(uint8_t wheel_mode);
    void        SetWheelSpeed(unsigned int speed);
    void        SetWheelBrightness(unsigned int brightness);

    /*-----------------------------------------------------*\
    | Power saving settings                                 |
    |   idle timeout is in units of 10 seconds              |
    \*-----------------------------------------------------*/
    void        SetLightsOffWhileMoving(bool enabled);
    void        SetIdleTimeout(uint8_t units_of_10s);

private:
    hid_device* dev;
    std::string location;
    std::string name;
    void*       win_handle;     /* Windows file handle used for HidD_SetOutputReport */

    int         main_light_enabled;     /* -1 unknown, 0 off, 1 on */
    int         wheel_enabled;          /* -1 unknown, 0 off, 1 on */
    long long   wheel_color;            /* -1 unknown, else 0xRRGGBB */
    unsigned int wheel_profile;         /* DPI profile the wheel color is written to */

    /* Last data sent to each address, used to skip identical writes */
    std::map<uint8_t, std::vector<uint8_t>> last_written;

    bool        WriteSetting(uint8_t address, const uint8_t* data, uint8_t data_len, bool dedupe = true);
    bool        SendFrame(const uint8_t* frame);
};