#pragma once

#include "BottomBar.hpp"
#include "EngineState.hpp"
#include "PlaneProfile.hpp"
#include "DisplayParams.hpp"

// Font includes used by draw functions
#include "Fonts/SevenSeg42.h"
#include "Fonts/SevenSeg32.h"
#include "Fonts/ArialN11.h"
#include "Fonts/ArialN16.h"
#include "Fonts/ArialNI16.h"
#include "Fonts/ArialNI36.h"

// ---------------------------------------------------------------------------
// Bottom bar draw functions — each one is a self-contained layout
// ---------------------------------------------------------------------------

// "LABEL value" — small label left, large value right (for dual mode)
inline void drawDualLabelValue(LGFX_Sprite &spr, const BottomValueDef &def,
                               int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    int16_t center = xLeft + 30;

    // Label (small)
    spr.loadFont(ArialN11);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MR_DATUM);
    spr.drawString(def.label, center - 1, h / 2);

    // Value (large)
    spr.loadFont(SevenSeg42);
    spr.setTextDatum(ML_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    if (def.decimals > 0)
        spr.drawFloat(curVal, def.decimals, center + 3, h / 2);
    else
        spr.drawNumber((int)curVal, center + 1, h / 2);
}

// "value UNITS" — large value left, large label/units right (for single mode)
inline void drawSingleValueUnits(LGFX_Sprite &spr, const BottomValueDef &def,
                                 int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    int16_t center = (xLeft + xRight) / 2;

    // Value (large)
    spr.loadFont(SevenSeg42);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    if (def.decimals > 0)
        spr.drawFloat(curVal, def.decimals, center - 3, h / 2);
    else
        spr.drawNumber((int)curVal, center - 3, h / 2);

    // Units/label (large)
    spr.loadFont(ArialNI36);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(ML_DATUM);
    spr.drawString(def.units ? def.units : def.label, center + 3, h / 2);
}

inline void drawHHMMValue(LGFX_Sprite &spr, const BottomValueDef &def,
                          int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    int16_t center = (xLeft + xRight) / 2;

    // Change value from minutes to hh:mm
    int hh = (int)((int)curVal / 60);
    int mm = (int)((int)curVal % 60);
    char buf[10];
    sprintf(buf, "%d:%02d",hh, mm);
    
    spr.loadFont(SevenSeg42);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    spr.drawString(buf, center - 3, h /2);

    spr.loadFont(ArialNI36);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(ML_DATUM);
    spr.drawString(def.units ? def.units : def.label, center + 3, h / 2);

}

// "LABEL value UNITS" — small label left, large value center, small units right
inline void drawLabelValueUnits(LGFX_Sprite &spr, const BottomValueDef &def,
                                int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    // int16_t center = xLeft + 30;
    int16_t center = spr.width() / 2;

    // Draw the value first because we need its width.

    // Value (large)
    spr.loadFont(SevenSeg42);
    // spr.setTextDatum(ML_DATUM);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    int16_t valX = center;
    int textWidth = 0;

    char buf[10];

    sprintf(buf, "%.f", def.decimals, curVal);
    spr.drawString(buf, valX, h / 2);
    textWidth = spr.textWidth(buf);

    // Label (small)
    spr.loadFont(ArialN16);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MR_DATUM);
    spr.drawString(def.label, center - textWidth / 2 - 3, h / 2);

    // Units (small, after value)
    if (def.units && def.units[0])
    {
        char buf[16];
        if (def.decimals > 0)
            snprintf(buf, sizeof(buf), "%.*f", def.decimals, (double)curVal);
        else
            snprintf(buf, sizeof(buf), "%d", (int)curVal);
        int16_t valWidth = spr.textWidth(buf);
        spr.loadFont(ArialN16);
        spr.setTextColor(TFT_WHITE);
        spr.setTextDatum(ML_DATUM);
        spr.drawString(def.units, valX + textWidth + 3, h / 2);
    }
}

// ---------------------------------------------------------------------------
// Helper: build a BottomValueDef from a GaugeRangeDef + data pointer.
//
// Copies up to 3 color ranges from the GaugeRangeDef (BottomValueDef only
// has room for 3, which is enough for value text coloring).
// ---------------------------------------------------------------------------
inline BottomValueDef makeValueDef(const char *label, const float *ptr,
                                   int decimals, const char *units,
                                   const GaugeRangeDef &range)
{
    BottomValueDef def = {};
    def.label = label;
    def.ptr = ptr;
    def.decimals = decimals;
    def.units = units;
    uint8_t n = range.colorCount < 3 ? range.colorCount : 3;
    for (uint8_t i = 0; i < n; i++)
    {
        def.colors[i] = range.colors[i];
    }
    def.colorCount = n;
    return def;
}

// ---------------------------------------------------------------------------
// buildBottomPages — populates a BottomPage array from the active profile.
//
// Only adds pages for measurements the profile says are available.
// Returns the number of pages written.
//
// The caller must provide an array large enough for all possible pages.
// MAX_BOTTOM_PAGES defines the upper bound.
//
// DrawMode, leftLabel, value parameters
// ---------------------------------------------------------------------------
static constexpr int MAX_BOTTOM_PAGES = 20;

inline int buildBottomPages(BottomPage *pages, const PlaneProfile &p, EngineState &state)
{
    int n = 0;

    // EGT / CHT selected cylinder (always present if EGT+CHT available)
    if (p.hasEgt && p.hasCht)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("EGT", &state.egtSelected, 0, nullptr, p.egt),
                      drawDualLabelValue,
                      makeValueDef("CHT", &state.chtSelected, 0, nullptr, p.cht)};
    }

    // EGT / CHT peak
    if (p.hasEgt && p.hasCht)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("EGT-P", &state.egtPeak, 0, nullptr, p.egt),
                      drawDualLabelValue,
                      makeValueDef("CHT-P", &state.chtPeak, 0, nullptr, p.cht)};
    }

    // TIT1
    if (p.hasTit1)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("TIT", &state.tit1, 0, nullptr, p.tit1),
                      nullptr,
                      {}};
    }

    // TIT2
    if (p.hasTit2)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("TIT2", &state.tit2, 0, nullptr, p.tit2),
                      nullptr,
                      {}};
    }

    // OAT
    if (p.hasOat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("OAT", &state.oat,
                                   decimalsFor(DisplayVarId::OAT, p, state), nullptr, p.oat),
                      nullptr,
                      {}};
    }

    // Fuel remaining
    if (p.hasUsed)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawLabelValueUnits,
                      makeValueDef("USD", &state.used,
                                   decimalsFor(DisplayVarId::FUEL_REM, p, state), "GAL", p.used),
                      nullptr,
                      {}};
    }

    // Fuel flow
    if (p.hasFf)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("FF", &state.ff,
                                   decimalsFor(DisplayVarId::FUEL_FLOW, p, state), "GPH", p.ff),
                      nullptr,
                      {}};
    }

    // %HP
    if (p.hasHp)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("% HP", &state.hp,
                                   decimalsFor(DisplayVarId::HP, p, state), nullptr, p.hp),
                      nullptr,
                      {}};
    }

    // Oil temp / Oil pressure (dual)
    if (p.hasOilT && p.hasOilP)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("O-T", &state.oilT,
                                   decimalsFor(DisplayVarId::OIL_TEMP, p, state), nullptr, p.oilT),
                      drawDualLabelValue,
                      makeValueDef("O-P", &state.oilP,
                                   decimalsFor(DisplayVarId::OIL_PRESS, p, state), nullptr, p.oilP)};
    }

    // Battery
    if (p.hasBat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawLabelValueUnits,
                      makeValueDef("BAT", &state.bat,
                                   decimalsFor(DisplayVarId::BATTERY, p, state), "V", p.bat),
                      nullptr,
                      {}};
    }

    // Carburetor air temp
    if (p.hasCrb)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("CRB", &state.crb,
                                   decimalsFor(DisplayVarId::CARB_TEMP, p, state), nullptr, p.crb),
                      nullptr,
                      {}};
    }

    // CDT / IAT (dual, turbo only)
    if (p.hasCdt && p.hasIat)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("CDT", &state.cdt,
                                   decimalsFor(DisplayVarId::CDT, p, state), nullptr, p.cdt),
                      drawDualLabelValue,
                      makeValueDef("IAT", &state.iat,
                                   decimalsFor(DisplayVarId::IAT, p, state), nullptr, p.iat)};
    }

    // EGT differential
    if (p.hasDif)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("DIF", &state.dif,
                                   decimalsFor(DisplayVarId::EGT_DIF, p, state), nullptr, p.dif),
                      nullptr,
                      {}};
    }

    // Fuel used (shares FUEL_REM decimals — same unit, gallons)
    if (p.hasUsed)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawLabelValueUnits,
                      makeValueDef("USD", &state.used,
                                   decimalsFor(DisplayVarId::FUEL_REM, p, state), "GAL", p.used),
                      nullptr,
                      {}};
    }

    // MPG
    if (p.hasMpg)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("MPG", &state.mpg, 1, nullptr, p.hp), // no specific range def for MPG yet
                      nullptr,
                      {}};
    }

    // Endurance
    if (p.hasEndurance)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawHHMMValue,
                      makeValueDef("REM", &state.endurance, 0, nullptr, p.endurance), // no specific range def yet
                      nullptr,
                      {}};
    }

    return n;
}
