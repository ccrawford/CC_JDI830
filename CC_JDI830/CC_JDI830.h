#pragma once

#include "Arduino.h"

class CC_JDI830
{
public:
    CC_JDI830(uint8_t SCLK, uint8_t MOSI, uint8_t DC, uint8_t CS, uint8_t RST, uint8_t BL);
    void begin();
    void attach();
    void detach();
    void set(int16_t messageID, char *setPoint);
    void update();

private:
    bool    _initialised;
    uint8_t _pinSCLK, _pinMOSI, _pinDC, _pinCS, _pinRST, _pinBL;
};