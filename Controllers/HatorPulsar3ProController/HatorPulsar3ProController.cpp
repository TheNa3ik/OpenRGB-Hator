/*---------------------------------------------------------*\
| HatorPulsar3ProController.cpp                             |
|                                                           |
|   Driver for Hator Pulsar 3 Pro wireless mouse            |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include "HatorPulsar3ProController.h"
#include "LogManager.h"
#include "StringUtils.h"

#ifdef _WIN32
#include <windows.h>
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#endif

HatorPulsar3ProController::HatorPulsar3ProController(hid_device* dev_handle, const char* path, std::string dev_name)
{
    dev                 = dev_handle;
    location            = path;
    name                = dev_name;
    win_handle          = nullptr;
    main_light_enabled  = -1;
    wheel_enabled       = -1;
    wheel_color         = -1;
    wheel_profile       = HATOR_DEFAULT_WHEEL_PROFILE;

#ifdef _WIN32
    /*-----------------------------------------------------*\
    | The mouse only accepts these commands as Output       |
    | reports sent with HidD_SetOutputReport, so open the   |
    | same HID collection with a Windows handle             |
    \*-----------------------------------------------------*/
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if(h == INVALID_HANDLE_VALUE)
    {
        LOG_WARNING("[Hator] CreateFile failed, error %lu", GetLastError());
    }
    else
    {
        win_handle = h;
    }
#endif
}

HatorPulsar3ProController::~HatorPulsar3ProController()
{
#ifdef _WIN32
    if(win_handle)
    {
        CloseHandle((HANDLE)win_handle);
    }
#endif

    hid_close(dev);
}

std::string HatorPulsar3ProController::GetName()
{
    return(name);
}

std::string HatorPulsar3ProController::GetLocation()
{
    return("HID: " + location);
}

std::string HatorPulsar3ProController::GetSerial()
{
    wchar_t serial_string[128];
    int ret = hid_get_serial_number_string(dev, serial_string, 128);

    if(ret != 0)
    {
        return("");
    }

    return(StringUtils::wstring_to_string(serial_string));
}

/*---------------------------------------------------------*\
| Low level: build a frame for one setting and send it      |
|   Returns false if the frame could not be sent            |
\*---------------------------------------------------------*/
bool HatorPulsar3ProController::WriteSetting(uint8_t address, const uint8_t* data, uint8_t data_len, bool dedupe)
{
    /* 6 header bytes + data + inner checksum must fit before the frame checksum */
    if(data_len > 9)
    {
        return(false);
    }

    /*-----------------------------------------------------*\
    | Skip the write if this address already holds exactly  |
    | this data (protects the mouse's memory and the link)  |
    \*-----------------------------------------------------*/
    std::vector<uint8_t> new_data(data, data + data_len);

    if(dedupe)
    {
        std::map<uint8_t, std::vector<uint8_t>>::iterator it = last_written.find(address);

        if((it != last_written.end()) && (it->second == new_data))
        {
            return(true);
        }
    }

    uint8_t frame[HATOR_PACKET_SIZE];
    memset(frame, 0x00, sizeof(frame));

    frame[0]    = HATOR_REPORT_ID;
    frame[1]    = HATOR_CMD_WRITE;
    frame[4]    = address;
    frame[5]    = (uint8_t)(data_len + 1);

    uint8_t sum = 0;

    for(uint8_t i = 0; i < data_len; i++)
    {
        frame[6 + i]    = data[i];
        sum             = (uint8_t)(sum + data[i]);
    }

    /* inner checksum */
    frame[6 + data_len] = (uint8_t)(HATOR_CHECKSUM_BASE - sum);

    /* frame checksum over bytes 0..15 */
    sum = 0;

    for(int i = 0; i < HATOR_PACKET_SIZE - 1; i++)
    {
        sum = (uint8_t)(sum + frame[i]);
    }

    frame[HATOR_PACKET_SIZE - 1] = (uint8_t)(HATOR_CHECKSUM_BASE - sum);

    /*-----------------------------------------------------*\
    | Only remember the data if the frame really went out,  |
    | so a lost write is sent again on the next update      |
    \*-----------------------------------------------------*/
    if(!SendFrame(frame))
    {
        return(false);
    }

    last_written[address] = new_data;

    return(true);
}

bool HatorPulsar3ProController::SendFrame(const uint8_t* frame)
{
    bool ok = false;

    char hex[HATOR_PACKET_SIZE * 3 + 1];

    for(int i = 0; i < HATOR_PACKET_SIZE; i++)
    {
        snprintf(hex + i * 3, 4, "%02x ", frame[i]);
    }

    LOG_DEBUG("[Hator] TX %s", hex);

#ifdef _WIN32
    if(win_handle)
    {
        /* HidD_SetOutputReport wants a non-const buffer */
        uint8_t buf[HATOR_PACKET_SIZE];
        memcpy(buf, frame, sizeof(buf));

        ok = (HidD_SetOutputReport((HANDLE)win_handle, buf, HATOR_PACKET_SIZE) != 0);

        if(!ok)
        {
            LOG_DEBUG("[Hator] HidD_SetOutputReport failed, error %lu", GetLastError());
        }
    }
#else
    ok = (hid_write(dev, frame, HATOR_PACKET_SIZE) >= 0);
#endif

    /* Give the mouse a moment between commands */
    std::this_thread::sleep_for(std::chrono::milliseconds(HATOR_FRAME_DELAY_MS));

    return(ok);
}

/*---------------------------------------------------------*\
| Underglow lighting                                        |
\*---------------------------------------------------------*/
void HatorPulsar3ProController::SetMainLightEnabled(bool enabled)
{
    int wanted = enabled ? 1 : 0;

    /* Skip if we already know the mouse is in this state */
    if(main_light_enabled == wanted)
    {
        return;
    }

    uint8_t data[1] = { (uint8_t)wanted };

    if(WriteSetting(HATOR_ADDR_MAIN_LIGHT_ENABLE, data, 1, false))
    {
        main_light_enabled = wanted;
    }
}

void HatorPulsar3ProController::SetMainEffect(uint8_t effect, uint8_t red, uint8_t grn, uint8_t blu,
                                              unsigned int speed, unsigned int brightness)
{
    /*-----------------------------------------------------*\
    | Clamp 1..10 and convert to 0-based indices            |
    \*-----------------------------------------------------*/
    uint8_t speed_idx   = (uint8_t)(std::clamp<unsigned int>(speed,      1, 10) - 1);
    uint8_t bright_idx  = (uint8_t)(std::clamp<unsigned int>(brightness, 1, 10) - 1);

    uint8_t data[6]     = { effect, red, grn, blu, speed_idx, bright_idx };
    WriteSetting(HATOR_ADDR_MAIN_LIGHT, data, 6);
}

/*---------------------------------------------------------*\
| Wheel (DPI) lighting                                      |
\*---------------------------------------------------------*/
void HatorPulsar3ProController::SetWheelProfile(unsigned int profile)
{
    unsigned int new_profile = std::min<unsigned int>(profile, HATOR_MAX_WHEEL_PROFILE);

    if(new_profile != wheel_profile)
    {
        wheel_profile   = new_profile;

        /* Force the color to be written again, to the new profile */
        wheel_color     = -1;
    }
}

void HatorPulsar3ProController::SetWheelEnabled(bool enabled)
{
    int wanted = enabled ? 1 : 0;

    if(wheel_enabled == wanted)
    {
        return;
    }

    uint8_t data[1] = { (uint8_t)wanted };

    if(WriteSetting(HATOR_ADDR_WHEEL_ENABLE, data, 1, false))
    {
        wheel_enabled = wanted;
    }
}

void HatorPulsar3ProController::SetWheelColor(uint8_t red, uint8_t grn, uint8_t blu)
{
    long long packed = ((long long)red << 16) | ((long long)grn << 8) | (long long)blu;

    if(wheel_color == packed)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | Select the configured DPI profile first (always sent, |
    | no dedupe), give the mouse time to switch, then write |
    | the profile's color                                   |
    \*-----------------------------------------------------*/
    uint8_t profile[1]  = { (uint8_t)wheel_profile };

    if(!WriteSetting(HATOR_ADDR_DPI_PROFILE, profile, 1, false))
    {
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(HATOR_PROFILE_SWITCH_DELAY_MS));

    uint8_t color[3]    = { red, grn, blu };

    if(WriteSetting((uint8_t)(HATOR_ADDR_DPI_COLOR_BASE + HATOR_DPI_COLOR_STRIDE * wheel_profile),
                     color, 3))
    {
        wheel_color = packed;
    }
}

void HatorPulsar3ProController::SetWheelMode(uint8_t wheel_mode)
{
    uint8_t data[1] = { wheel_mode };
    WriteSetting(HATOR_ADDR_WHEEL_MODE, data, 1);
}

void HatorPulsar3ProController::SetWheelSpeed(unsigned int speed)
{
    uint8_t data[1] = { (uint8_t)std::clamp<unsigned int>(speed, 1, HATOR_WHEEL_SPEED_MAX) };
    WriteSetting(HATOR_ADDR_WHEEL_SPEED, data, 1);
}

void HatorPulsar3ProController::SetWheelBrightness(unsigned int brightness)
{
    /* Map 1..10 to 0x10..0xFF */
    unsigned int b      = std::clamp<unsigned int>(brightness, 1, 10);
    uint8_t      value  = (uint8_t)(0x10 + ((b - 1) * (0xFF - 0x10)) / 9);

    uint8_t data[1]     = { value };
    WriteSetting(HATOR_ADDR_WHEEL_BRIGHTNESS, data, 1);
}

/*---------------------------------------------------------*\
| Power saving settings                                     |
\*---------------------------------------------------------*/
void HatorPulsar3ProController::SetLightsOffWhileMoving(bool enabled)
{
    uint8_t data[1] = { (uint8_t)(enabled ? 1 : 0) };
    WriteSetting(HATOR_ADDR_LIGHTS_OFF_MOVING, data, 1, false);
}

void HatorPulsar3ProController::SetIdleTimeout(uint8_t units_of_10s)
{
    uint8_t data[1] = { units_of_10s };
    WriteSetting(HATOR_ADDR_IDLE_TIMEOUT, data, 1, false);
}