/*---------------------------------------------------------*\
| RGBController_HatorPulsar3Pro.h                           |
|                                                           |
|   RGBController for Hator Pulsar 3 Pro                    |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBController.h"
#include "HatorPulsar3ProController.h"

class RGBController_HatorPulsar3Pro : public RGBController
{
public:
    RGBController_HatorPulsar3Pro(HatorPulsar3ProController* controller_ptr);
    ~RGBController_HatorPulsar3Pro();

    void        SetupZones();
    void        SetupModes();
    void        LoadSettings();

    void        DeviceUpdateLEDs();
    void        DeviceUpdateZoneLEDs(int zone);
    void        DeviceUpdateSingleLED(int led);

    void        DeviceUpdateMode();
    void        DeviceUpdateZoneMode(int zone);

private:
    HatorPulsar3ProController* controller;

    void        ApplyMainMode(const mode& m);
    void        ApplyWheelMode(const mode& m);
    void        ApplyWheelFromDeviceMode(const mode& m);
    void        ApplyWheel(int wheel_mode, RGBColor color, unsigned int speed, unsigned int brightness);
};