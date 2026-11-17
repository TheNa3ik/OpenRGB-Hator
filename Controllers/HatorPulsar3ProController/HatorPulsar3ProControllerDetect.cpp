/*---------------------------------------------------------*\
| HatorPulsar3ProControllerDetect.cpp                       |
|                                                           |
|   Detector for Hator Pulsar 3 Pro                         |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <vector>
#include <hidapi.h>
#include "HatorPulsar3ProController.h"
#include "DetectionManager.h"
#include "RGBController_HatorPulsar3Pro.h"

/*---------------------------------------------------------*\
| Vendor collection on HID interface 1 that accepts the     |
| 17-byte Output report (ID 0x08). The wireless dongle and  |
| the USB cable expose the same collection.                 |
\*---------------------------------------------------------*/
#define HATOR_INTERFACE     1
#define HATOR_USAGE_PAGE    0xFF02
#define HATOR_USAGE         0x0002

/* Detector name of the dongle, used to find its controller */
#define HATOR_DONGLE_NAME   "Hator Pulsar 3 Pro"

DetectedControllers DetectHatorPulsar3ProControllers(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected_controllers;
    hid_device*         dev;

    dev = hid_open_path(info->path);

    if(dev)
    {
        HatorPulsar3ProController*     controller       = new HatorPulsar3ProController(dev, info->path, name);
        RGBController_HatorPulsar3Pro* rgb_controller   = new RGBController_HatorPulsar3Pro(controller);

        detected_controllers.push_back(rgb_controller);
    }

    return(detected_controllers);
}

/*---------------------------------------------------------*\
| When the USB cable is plugged in, ignore the dongle so    |
| the mouse is not listed twice (the cable wins). This is   |
| checked when OpenRGB detects devices (startup or Rescan). |
\*---------------------------------------------------------*/
static bool HatorUsbPresent()
{
    hid_device_info* devs   = hid_enumerate(HATOR_VID, HATOR_PULSAR_3_PRO_USB_PID);
    bool             found  = (devs != NULL);

    hid_free_enumeration(devs);

    return(found);
}

/*---------------------------------------------------------*\
| Remove the dongle's controller from the device list. Used |
| when the cable is plugged in while OpenRGB is running.    |
| Same pattern as DetectionManager's unplug callback: find  |
| the controllers under the lock, then unregister and       |
| delete them after releasing it.                           |
\*---------------------------------------------------------*/
static void HatorRemoveDongleControllers()
{
    DetectionManager*           dm = DetectionManager::get();
    std::vector<RGBController*> to_remove;

    dm->LockRGBControllers();

    std::vector<RGBController*>& list = dm->GetRGBControllers();

    for(RGBController* c : list)
    {
        if(c->GetName() == HATOR_DONGLE_NAME)
        {
            to_remove.push_back(c);
        }
    }

    dm->UnlockRGBControllers();

    for(RGBController* c : to_remove)
    {
        dm->UnregisterRGBController(c);
        delete c;
    }
}

DetectedControllers DetectHatorPulsar3ProDongle(hid_device_info* info, const std::string& name)
{
    if(HatorUsbPresent())
    {
        return(DetectedControllers());
    }

    return(DetectHatorPulsar3ProControllers(info, name));
}

DetectedControllers DetectHatorPulsar3ProUsb(hid_device_info* info, const std::string& name)
{
    DetectedControllers detected = DetectHatorPulsar3ProControllers(info, name);

    /* The cable is now in use, so the dongle entry would be a duplicate */
    if(!detected.empty())
    {
        HatorRemoveDongleControllers();
    }

    return(detected);
}

REGISTER_HID_DETECTOR_IPU("Hator Pulsar 3 Pro",        DetectHatorPulsar3ProDongle, HATOR_VID, HATOR_PULSAR_3_PRO_PID,     HATOR_INTERFACE, HATOR_USAGE_PAGE, HATOR_USAGE);
REGISTER_HID_DETECTOR_IPU("Hator Pulsar 3 Pro (USB)",  DetectHatorPulsar3ProUsb,    HATOR_VID, HATOR_PULSAR_3_PRO_USB_PID, HATOR_INTERFACE, HATOR_USAGE_PAGE, HATOR_USAGE);