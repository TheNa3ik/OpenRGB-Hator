/*---------------------------------------------------------*\
| RGBController_HatorPulsar3Pro.cpp                         |
|                                                           |
|   RGBController for Hator Pulsar 3 Pro                    |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <cstdlib>
#include "RGBController_HatorPulsar3Pro.h"
#include "ResourceManager.h"
#include "SettingsManager.h"

/**------------------------------------------------------------------*\
    @name Hator Pulsar 3 Pro
    @category Mouse
    @type USB
    @save :robot:
    @direct :x:
    @effects :white_check_mark:
    @detectors DetectHatorPulsar3ProControllers
    @comment Works with the wireless dongle and the USB cable.
        The underglow light and the scroll wheel are separate zones.
        The wheel shows the color of one DPI profile, selected in the
        "HatorPulsar3Pro" section of OpenRGB.json:
        wheel_dpi_profile (1-6), lights_off_while_moving (bool),
        idle_timeout_seconds.
        While the USB cable is charging the mouse, the mouse shows
        its own battery indicator on the underglow and ignores
        underglow commands. The wheel is not affected.
\*-------------------------------------------------------------------*/

#define HATOR_ZONE_MAIN         0
#define HATOR_ZONE_WHEEL        1

#define HATOR_SETTINGS_KEY      "HatorPulsar3Pro"

/*---------------------------------------------------------*\
| Idle lights-off timer options (value in units of 10 s)    |
\*---------------------------------------------------------*/
static const uint8_t hator_idle_units[] = { 1, 3, 6, 30, 60, 90, 120, 150, 180, 210, 240 };

/*---------------------------------------------------------*\
| Build one mode entry. The mode "value" is the byte sent   |
| to the mouse (or HATOR_MODE_OFF).                         |
\*---------------------------------------------------------*/
static mode MakeMode(const char* name, int value, unsigned int flags, bool has_color, unsigned int speed_max)
{
    mode m;

    m.name              = name;
    m.value             = value;
    m.flags             = flags;

    if(flags & MODE_FLAG_HAS_SPEED)
    {
        m.speed_min     = 1;
        m.speed_max     = speed_max;
        m.speed         = (speed_max < 5) ? 3 : 5;
    }

    if(flags & MODE_FLAG_HAS_BRIGHTNESS)
    {
        m.brightness_min    = 1;
        m.brightness_max    = 10;
        m.brightness        = 10;
    }

    if(has_color)
    {
        m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
        m.colors_min    = 1;
        m.colors_max    = 1;
        m.colors.resize(1);
        m.colors[0]     = ToRGBColor(255, 0, 0);
    }
    else
    {
        m.color_mode    = MODE_COLORS_NONE;
    }

    return m;
}

RGBController_HatorPulsar3Pro::RGBController_HatorPulsar3Pro(HatorPulsar3ProController* controller_ptr)
{
    controller                  = controller_ptr;

    name                        = controller->GetName();
    vendor                      = "Hator";
    type                        = DEVICE_TYPE_MOUSE;
    description                 = "Hator Pulsar 3 Pro Wireless Mouse";
    serial                      = controller->GetSerial();
    location                    = controller->GetLocation();

    /* Zones first: SetupModes() adds the per-zone mode lists to them */
    SetupZones();
    SetupModes();
    LoadSettings();
}

RGBController_HatorPulsar3Pro::~RGBController_HatorPulsar3Pro()
{
    Shutdown();

    delete controller;
}

/*---------------------------------------------------------*\
| Read the optional "HatorPulsar3Pro" section of the        |
| OpenRGB settings file. A key that is missing is never     |
| sent to the mouse, because it remembers its own settings. |
\*---------------------------------------------------------*/
void RGBController_HatorPulsar3Pro::LoadSettings()
{
    SettingsManager* settings_manager = ResourceManager::get()->GetSettingsManager();

    if(settings_manager == NULL)
    {
        return;
    }

    nlohmann::json settings = settings_manager->GetSettings(HATOR_SETTINGS_KEY);

    /*-----------------------------------------------------*\
    | Wheel DPI profile, 1..6 in the file, 0-based inside   |
    \*-----------------------------------------------------*/
    if(settings.contains("wheel_dpi_profile") && settings["wheel_dpi_profile"].is_number_integer())
    {
        int profile = settings["wheel_dpi_profile"].get<int>();

        profile = std::clamp(profile, 1, HATOR_MAX_WHEEL_PROFILE + 1);

        controller->SetWheelProfile((unsigned int)(profile - 1));
    }

    /*-----------------------------------------------------*\
    | Lights off while moving                               |
    \*-----------------------------------------------------*/
    if(settings.contains("lights_off_while_moving") && settings["lights_off_while_moving"].is_boolean())
    {
        controller->SetLightsOffWhileMoving(settings["lights_off_while_moving"].get<bool>());
    }

    /*-----------------------------------------------------*\
    | Idle timer: pick the closest option the mouse has     |
    \*-----------------------------------------------------*/
    if(settings.contains("idle_timeout_seconds") && settings["idle_timeout_seconds"].is_number_integer())
    {
        int seconds = settings["idle_timeout_seconds"].get<int>();
        int best    = 0;

        for(size_t i = 1; i < sizeof(hator_idle_units) / sizeof(hator_idle_units[0]); i++)
        {
            if(std::abs(seconds - hator_idle_units[i] * 10) < std::abs(seconds - hator_idle_units[best] * 10))
            {
                best = (int)i;
            }
        }

        controller->SetIdleTimeout(hator_idle_units[best]);
    }
}

void RGBController_HatorPulsar3Pro::SetupModes()
{
    const unsigned int speed_bright = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS;
    const unsigned int color_flag   = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR;

    /*-----------------------------------------------------*\
    | Underglow modes, same order as in the Hator software  |
    | They are used both as the device-level mode list and  |
    | as the mode list of the "Underglow" zone              |
    \*-----------------------------------------------------*/
    std::vector<mode> main_modes;

    main_modes.push_back(MakeMode("Off",               HATOR_MODE_OFF,                 0,                                          false, 10));
    main_modes.push_back(MakeMode("Breathing",         HATOR_EFFECT_BREATHING,         speed_bright | color_flag,                  true,  10));
    main_modes.push_back(MakeMode("Static",            HATOR_EFFECT_STATIC,            MODE_FLAG_HAS_BRIGHTNESS | color_flag,      true,  10));
    main_modes.push_back(MakeMode("Neon",              HATOR_EFFECT_NEON,              speed_bright,                               false, 10));
    main_modes.push_back(MakeMode("Rainbow Breathing", HATOR_EFFECT_RAINBOW_BREATHING, speed_bright,                               false, 10));
    main_modes.push_back(MakeMode("Rainbow Wave",      HATOR_EFFECT_RAINBOW_WAVE,      speed_bright,                               false, 10));
    main_modes.push_back(MakeMode("Colorful",          HATOR_EFFECT_COLORFUL,          MODE_FLAG_HAS_BRIGHTNESS,                   false, 10));

    modes                           = main_modes;
    zones[HATOR_ZONE_MAIN].modes    = main_modes;

    /*-----------------------------------------------------*\
    | Wheel modes. Fixed: color + brightness.               |
    | Breathing: color + speed (1..5).                      |
    \*-----------------------------------------------------*/
    zones[HATOR_ZONE_WHEEL].modes.push_back(MakeMode("Off",       HATOR_MODE_OFF,         0,                                         false, 5));
    zones[HATOR_ZONE_WHEEL].modes.push_back(MakeMode("Fixed",     HATOR_WHEEL_FIXED,      MODE_FLAG_HAS_BRIGHTNESS | color_flag,     true,  5));
    zones[HATOR_ZONE_WHEEL].modes.push_back(MakeMode("Breathing", HATOR_WHEEL_BREATHING,  MODE_FLAG_HAS_SPEED | color_flag,          true,  HATOR_WHEEL_SPEED_MAX));

    /*-----------------------------------------------------*\
    | The mouse remembers its own state and we cannot read  |
    | it back, so these are only the GUI's starting point   |
    \*-----------------------------------------------------*/
    active_mode                             = 2;    /* Static */
    zones[HATOR_ZONE_MAIN].active_mode      = 2;    /* Static */
    zones[HATOR_ZONE_WHEEL].active_mode     = 1;    /* Fixed  */
}

void RGBController_HatorPulsar3Pro::SetupZones()
{
    leds.clear();
    colors.clear();
    zones.clear();

    zone main_zone;
    main_zone.name              = "Underglow";
    main_zone.type              = ZONE_TYPE_SINGLE;
    main_zone.leds_min          = 1;
    main_zone.leds_max          = 1;
    main_zone.leds_count        = 1;
    zones.push_back(main_zone);

    zone wheel_zone;
    wheel_zone.name             = "Wheel";
    wheel_zone.type             = ZONE_TYPE_SINGLE;
    wheel_zone.leds_min         = 1;
    wheel_zone.leds_max         = 1;
    wheel_zone.leds_count       = 1;
    zones.push_back(wheel_zone);

    led main_led;
    main_led.name               = "Underglow LED";
    main_led.value              = HATOR_ZONE_MAIN;
    leds.push_back(main_led);

    led wheel_led;
    wheel_led.name              = "Wheel LED";
    wheel_led.value             = HATOR_ZONE_WHEEL;
    leds.push_back(wheel_led);

    SetupColors();
}

void RGBController_HatorPulsar3Pro::ApplyMainMode(const mode& m)
{
    if(m.value == HATOR_MODE_OFF)
    {
        controller->SetMainLightEnabled(false);
        return;
    }

    /*-----------------------------------------------------*\
    | Preset modes ignore the color bytes, so send white    |
    | for them. Breathing and Static use their own color.   |
    \*-----------------------------------------------------*/
    RGBColor c = ToRGBColor(255, 255, 255);

    if(m.color_mode == MODE_COLORS_MODE_SPECIFIC && !m.colors.empty())
    {
        c = m.colors[0];
    }

    controller->SetMainEffect((uint8_t)m.value,
                              RGBGetRValue(c), RGBGetGValue(c), RGBGetBValue(c),
                              m.speed, m.brightness);

    /* Turn the lights on (only sent if they were off) */
    controller->SetMainLightEnabled(true);
}

/*---------------------------------------------------------*\
| Send wheel settings. wheel_mode is HATOR_MODE_OFF,        |
| HATOR_WHEEL_FIXED or HATOR_WHEEL_BREATHING                |
\*---------------------------------------------------------*/
void RGBController_HatorPulsar3Pro::ApplyWheel(int wheel_mode, RGBColor color, unsigned int speed, unsigned int brightness)
{
    if(wheel_mode == HATOR_MODE_OFF)
    {
        controller->SetWheelEnabled(false);
        return;
    }

    controller->SetWheelColor(RGBGetRValue(color), RGBGetGValue(color), RGBGetBValue(color));
    controller->SetWheelMode((uint8_t)wheel_mode);

    if(wheel_mode == HATOR_WHEEL_BREATHING)
    {
        controller->SetWheelSpeed(speed);
    }
    else
    {
        controller->SetWheelBrightness(brightness);
    }

    controller->SetWheelEnabled(true);
}

/* The wheel zone has its own active mode */
void RGBController_HatorPulsar3Pro::ApplyWheelMode(const mode& m)
{
    RGBColor c = ToRGBColor(255, 255, 255);

    if(m.color_mode == MODE_COLORS_MODE_SPECIFIC && !m.colors.empty())
    {
        c = m.colors[0];
    }

    ApplyWheel(m.value, c, m.speed, m.brightness);
}

/*---------------------------------------------------------*\
| The wheel follows the device-level (underglow) mode       |
|   Off       -> wheel off                                  |
|   Static    -> wheel Fixed, same color and brightness     |
|   Breathing -> wheel Breathing, same color, speed 1..10   |
|                mapped to 1..5                             |
|   others    -> the wheel has no equivalent, leave it as is|
\*---------------------------------------------------------*/
void RGBController_HatorPulsar3Pro::ApplyWheelFromDeviceMode(const mode& m)
{
    RGBColor c = ToRGBColor(255, 255, 255);

    if(m.color_mode == MODE_COLORS_MODE_SPECIFIC && !m.colors.empty())
    {
        c = m.colors[0];
    }

    switch(m.value)
    {
    case HATOR_MODE_OFF:
        ApplyWheel(HATOR_MODE_OFF, c, 0, 0);
        break;

    case HATOR_EFFECT_STATIC:
        ApplyWheel(HATOR_WHEEL_FIXED, c, 0, m.brightness);
        break;

    case HATOR_EFFECT_BREATHING:
        ApplyWheel(HATOR_WHEEL_BREATHING, c, (m.speed + 1) / 2, 0);
        break;

    default:
        break;
    }
}

void RGBController_HatorPulsar3Pro::DeviceUpdateLEDs()
{
    /* All settings are hardware modes, so there is nothing per-LED to send */
    DeviceUpdateMode();
}

void RGBController_HatorPulsar3Pro::DeviceUpdateZoneLEDs(int zone)
{
    DeviceUpdateZoneMode(zone);
}

void RGBController_HatorPulsar3Pro::DeviceUpdateSingleLED(int led)
{
    if((led >= 0) && ((size_t)led < leds.size()))
    {
        DeviceUpdateZoneMode((int)leds[led].value);
    }
}

void RGBController_HatorPulsar3Pro::DeviceUpdateMode()
{
    DeviceUpdateZoneMode(HATOR_ZONE_MAIN);
    DeviceUpdateZoneMode(HATOR_ZONE_WHEEL);
}

void RGBController_HatorPulsar3Pro::DeviceUpdateZoneMode(int zone)
{
    if((zone < 0) || ((size_t)zone >= zones.size()))
    {
        return;
    }

    /*-----------------------------------------------------*\
    | A zone's own active mode wins; otherwise ("follow     |
    | device mode") use the device-level mode               |
    \*-----------------------------------------------------*/
    int  zone_mode      = zones[zone].active_mode;
    bool has_zone_mode  = (zone_mode >= 0) && ((size_t)zone_mode < zones[zone].modes.size());
    bool has_dev_mode   = (active_mode >= 0) && ((size_t)active_mode < modes.size());

    if(zone == HATOR_ZONE_MAIN)
    {
        if(has_zone_mode)
        {
            ApplyMainMode(zones[zone].modes[zone_mode]);
        }
        else if(has_dev_mode)
        {
            ApplyMainMode(modes[active_mode]);
        }
    }
    else if(zone == HATOR_ZONE_WHEEL)
    {
        if(has_zone_mode)
        {
            ApplyWheelMode(zones[zone].modes[zone_mode]);
        }
        else if(has_dev_mode)
        {
            ApplyWheelFromDeviceMode(modes[active_mode]);
        }
    }
}