#pragma once

#include <Arduino.h>
#include "CC_JDI830.h"

// only one entry required if you have only one custom device
enum {
    JDI830_DEVICE = 1
};
class MFCustomDevice
{
public:
    MFCustomDevice();
    void attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash = false);
    void detach();
    void update();
    void set(int16_t messageID, char *setPoint);

private:
    bool           getStringFromMem(uint16_t addreeprom, char *buffer, bool configFromFlash);
    bool           _initialized = false;
    CC_JDI830 *_mydevice;
    uint8_t        _pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL;
    uint8_t        _customType = 0;
};
