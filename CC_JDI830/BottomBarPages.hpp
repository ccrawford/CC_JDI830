#pragma once

#include "BottomBar.hpp"
#include "AlarmManager.hpp"
#include "EngineState.hpp"
#include "PlaneProfile.hpp"
#include "DisplayParams.hpp"

// Font includes used by draw functions
#include "Fonts/SevenSeg42.h"
#include "Fonts/SevenSeg32.h"
#include "Fonts/ArialN11.h"
#include "Fonts/ArialN16.h"
#include "Fonts/ArialNI16.h"
#include "Fonts/ArialNI20.h"
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

    // Label (small) — prepend dot if excluded from AUTO rotation
    spr.loadFont(ArialN11);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MR_DATUM);
    if (def.excluded) {
        char lbl[10];
        snprintf(lbl, sizeof(lbl), ".%s", def.label);
        spr.drawString(lbl, center - 1, h / 2);
    } else {
        spr.drawString(def.label, center - 1, h / 2);
    }

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

    // Units/label (large) — prepend dot to label if excluded from AUTO rotation
    spr.loadFont(ArialNI36);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(ML_DATUM);
    if (!def.units && def.excluded) {
        char lbl[10];
        snprintf(lbl, sizeof(lbl), ".%s", def.label);
        spr.drawString(lbl, center + 3, h / 2);
    } else {
        spr.drawString(def.units ? def.units : def.label, center + 3, h / 2);
    }
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

    // Units/label — prepend dot to label if excluded from AUTO rotation
    spr.loadFont(ArialNI36);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(ML_DATUM);
    if (!def.units && def.excluded) {
        char lbl[10];
        snprintf(lbl, sizeof(lbl), ".%s", def.label);
        spr.drawString(lbl, center + 3, h / 2);
    } else {
        spr.drawString(def.units ? def.units : def.label, center + 3, h / 2);
    }
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

    snprintf(buf, sizeof(buf), "%.*f", def.decimals, (double)curVal);
    spr.drawString(buf, valX, h / 2);
    textWidth = spr.textWidth(buf);

    // Label (small) — prepend dot if excluded from AUTO rotation
    spr.loadFont(ArialN16);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MR_DATUM);
    if (def.excluded) {
        char lbl[10];
        snprintf(lbl, sizeof(lbl), ".%s", def.label);
        spr.drawString(lbl, center - textWidth / 2 - 3, h / 2);
    } else {
        spr.drawString(def.label, center - textWidth / 2 - 3, h / 2);
    }

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
// drawCalculatedValue — "Calculated" label in ArialN11 top-center, then
// value in SevenSeg32 + units/label below.  Used for computed values like
// fuel remaining, fuel reserve, fuel used, fuel required, intercooler eff.
//
// Layout (within the sprite height):
//   top ~1/3:  "Calculated" small centered text
//   bottom 2/3: value + units (same layout as drawSingleValueUnits but smaller font)
// ---------------------------------------------------------------------------
inline void drawCalculatedValue(LGFX_Sprite &spr, const BottomValueDef &def,
                                int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    int16_t center = (xLeft + xRight) / 2;
    int16_t topY   = h / 5;         // "Calculated" label near the top
    int16_t valY   = h / 2 + h / 8; // value shifted down to make room

    // "Calculated" label (small)
    spr.loadFont(ArialN11);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Calculated", center+4, 0);
    

    // Value (smaller than normal to leave room for label above)
    spr.loadFont(SevenSeg42);
    spr.setTextDatum(BR_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    if (def.decimals > 0)
        spr.drawFloat(curVal, def.decimals, center - 4, h);
    else
        spr.drawNumber((int)curVal, center - 4, h);

    // Units/label
    spr.loadFont(ArialNI20);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(BL_DATUM);
    spr.drawString(def.units ? def.units : def.label, center + 12, h+2);
}

// ---------------------------------------------------------------------------
// drawCalculatedHHMM — same as drawCalculatedValue but formats the value
// as H:MM (minutes → hours:minutes).  Used for endurance.
// xLeft typically 0 xRight _w h: _h
// ---------------------------------------------------------------------------
inline void drawCalculatedHHMM(LGFX_Sprite &spr, const BottomValueDef &def,
                               int16_t xLeft, int16_t xRight, int16_t h)
{
    float curVal = def.ptr ? *def.ptr : 0;
    int16_t center = (xLeft + xRight) / 2;
    int16_t topY   = h / 5;
    int16_t valY   = h / 2 + h / 8;

    // "Calculated" label
    spr.loadFont(ArialN11);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(TL_DATUM);
    spr.drawString("Calculated", center+4, 0);

    // H:MM value
    int hh = (int)((int)curVal / 60);
    int mm = (int)((int)curVal % 60);
    char buf[10];
    sprintf(buf, "%d:%02d", hh, mm);

    spr.loadFont(SevenSeg42);
    spr.setTextDatum(BR_DATUM);
    spr.setTextColor(colorForDef(def, curVal));
    spr.drawString(buf, center - 4, h);

    // Units/label
    spr.loadFont(ArialNI20);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(BL_DATUM);
    spr.drawString(def.units ? def.units : def.label, center + 12, h+2);
}

// ---------------------------------------------------------------------------
// Alarm draw — renders the alarm layout in the bottom bar sprite.
//
// Layout:
//   Left half:  current value in WHITE, large SevenSeg42 font
//               (H:MM format for endurance alarms)
//   Right half: alarm label in RED, large ArialNI36 font (flashes)
//               + cylinder number in RED SevenSeg42 (CHT only)
//
// The flash state (on/off) is managed by BottomBar::update() and read
// from _alarmFlashOn.  When the label is "off" during the flash cycle,
// only the value is drawn — the right side stays black.
// ---------------------------------------------------------------------------
inline void drawAlarmPage(LGFX_Sprite& spr, const AlarmDef& alarm,
                          bool flashOn, int16_t w, int16_t h)
{
    float val = alarm.valuePtr ? *alarm.valuePtr : 0;
    int16_t midX = w / 2;
    int16_t midY = h / 2;

    int valueX = midX + midX /2; // Right half
    int labelX = midX / 2;


    // --- Left side: value in white ---
    spr.loadFont(SevenSeg42);
    spr.setTextColor(TFT_WHITE);
    spr.setTextDatum(MC_DATUM);

    if (alarm.timeFormat) {
        // Endurance: show as H:MM
        int totalMin = static_cast<int>(val);
        int hours = totalMin / 60;
        int mins  = totalMin % 60;
        char buf[10];
        snprintf(buf, sizeof(buf), "%d:%02d", hours, mins);
        spr.drawString(buf, midX / 2, midY);
    } else if (alarm.decimals > 0) {
        spr.drawFloat(val, alarm.decimals, midX / 2, midY);
    } else {
        spr.drawNumber(static_cast<int>(val), midX / 2, midY);
    }

    // --- Right side: flashing label in red ---
    if (flashOn) {
        // Label (e.g. "CHT", "OIL", "BAT")
        spr.loadFont(ArialNI36);
        spr.setTextColor(TFT_RED);


        if (alarm.cylPtr) {
            // CHT alarm: label + cylinder number side by side
            // Draw label slightly left of center-right, cylinder number after it
            spr.setTextDatum(MR_DATUM);
            spr.drawString(alarm.label, midX + (midX / 2) - 2, midY);

            // Cylinder number (1-based) in SevenSeg42
            spr.loadFont(SevenSeg42);
            spr.setTextColor(TFT_RED);
            spr.setTextDatum(ML_DATUM);
            spr.drawNumber(*alarm.cylPtr + 1, midX + (midX / 2) + 2, midY);
        } else {
            // All other alarms: label centered in right half
            spr.setTextDatum(MC_DATUM);
            spr.drawString(alarm.label, midX + midX / 2, midY);
        }
    }

}

// ---------------------------------------------------------------------------
// BottomBar::drawAlarm — delegates to the free function drawAlarmPage().
//
// This is defined here (not in BottomBar.hpp) because it needs the full
// AlarmDef type and the font headers.  This is the same pattern used for
// PlaneProfile::isAvailable() — declare in the header, define in a later
// file that has the necessary includes.
// ---------------------------------------------------------------------------
inline void BottomBar::drawAlarm() {
    if (_alarmDef) {
        drawAlarmPage(_sprite, *_alarmDef, _alarmFlashOn, _w, _h);
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
static constexpr int MAX_BOTTOM_PAGES = 24;

// ---------------------------------------------------------------------------
// buildBottomPages — populates a BottomPage array from the active profile.
//
// Only adds pages for measurements the profile says are available.
// Returns the number of pages written.
//
// Page order follows the EDM830 manual scan sequence.  Each page is tagged
// with a ScanGroup so the 3-position select switch can filter them:
//   TEMP = shown in EGT (temperature) and ALL positions
//   FUEL = shown in FF (fuel flow) and ALL positions
//   BOTH = shown in all three switch positions
// ---------------------------------------------------------------------------
inline int buildBottomPages(BottomPage *pages, const PlaneProfile &p, EngineState &state)
{
    int n = 0;

    // ======= TEMPERATURE GROUP (TEMP) — first block =======

    // Battery voltage
    if (p.hasBat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawLabelValueUnits,
                      makeValueDef("BAT", &state.bat,
                                   decimalsFor(DisplayVarId::BATTERY, p, state), "V", p.bat),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // Outside air temp
    if (p.hasOat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("OAT", &state.oat,
                                   decimalsFor(DisplayVarId::OAT, p, state), nullptr, p.oat),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // Intercooler air temp (turbo only)
    if (p.hasIat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("IAT", &state.iat,
                                   decimalsFor(DisplayVarId::IAT, p, state), nullptr, p.iat),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // Compressor discharge temp (turbo only)
    if (p.hasCdt)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("CDT", &state.cdt,
                                   decimalsFor(DisplayVarId::CDT, p, state), nullptr, p.cdt),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // Intercooler efficiency (calculated: CDT - IAT)
    if (p.hasCdt && p.hasIat)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("C-I", &state.cdtLessIat, 0, nullptr, p.intercoolerEff),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // Carburetor air temp (NA engines only — mutually exclusive with CDT/IAT)
    if (p.hasCrb)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("CRB", &state.crb,
                                   decimalsFor(DisplayVarId::CARB_TEMP, p, state), nullptr, p.crb),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // EGT differential (hottest - coldest)
    if (p.hasDif)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("DIF", &state.dif,
                                   decimalsFor(DisplayVarId::EGT_DIF, p, state), nullptr, p.dif),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // ======= BOTH GROUP — visible in all switch positions =======

    // RPM / MAP dual page
    if (p.hasMap)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("RPM", &state.rpm, 0, nullptr, p.rpm),
                      drawDualLabelValue,
                      makeValueDef("MAP", &state.map,
                                   decimalsFor(DisplayVarId::MAP, p, state), nullptr, p.map),
                      false, ScanGroup::BOTH};
    }

    // ======= FUEL GROUP (FUEL) =======

    // Fuel remaining (calculated)
    if (p.hasFuelRem)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawCalculatedValue,
                      makeValueDef("REM", &state.fuelRem,
                                   decimalsFor(DisplayVarId::FUEL_REM, p, state), "GAL", p.fuelRem),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // Fuel required to waypoint (calculated)
    if (p.hasReq)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("REQ", &state.req,
                                   decimalsFor(DisplayVarId::FUEL_REQ, p, state), nullptr, p.req),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // Fuel reserve at waypoint (calculated)
    if (p.hasRes)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawCalculatedValue,
                      makeValueDef("REM", &state.res,
                                   decimalsFor(DisplayVarId::FUEL_RES, p, state), nullptr, p.res),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // MPG (nautical miles per gallon)
    if (p.hasMpg)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("MPG", &state.mpg, 1, nullptr, p.ff),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // Time to empty / endurance (calculated, H:MM format)
    if (p.hasEndurance)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawCalculatedHHMM,
                      makeValueDef("H:M", &state.endurance, 0, nullptr, p.endurance),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // Fuel flow rate
    if (p.hasFf)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("FF", &state.ff,
                                   decimalsFor(DisplayVarId::FUEL_FLOW, p, state), "GPH", p.ff),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // Fuel used (calculated)
    if (p.hasUsed)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("USD", &state.used,
                                   decimalsFor(DisplayVarId::FUEL_USED, p, state), nullptr, p.used),
                      nullptr, {},
                      false, ScanGroup::FUEL};
    }

    // ======= TEMPERATURE GROUP (TEMP) — continued =======

    // EGT / CHT selected cylinder
    if (p.hasEgt && p.hasCht)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("EGT", &state.egtSelected, 0, nullptr, p.egt),
                      drawDualLabelValue,
                      makeValueDef("CHT", &state.chtSelected, 0, nullptr, p.cht),
                      true, ScanGroup::TEMP};  // nonExcludable
    }

    // EGT / CHT peak
    if (p.hasEgt && p.hasCht)
    {
        pages[n++] = {BottomMode::DUAL,
                      drawDualLabelValue,
                      makeValueDef("EGT-P", &state.egtPeak, 0, nullptr, p.egt),
                      drawDualLabelValue,
                      makeValueDef("CHT-P", &state.chtPeak, 0, nullptr, p.cht),
                      true, ScanGroup::TEMP};  // nonExcludable
    }

    // TIT1
    if (p.hasTit1)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("TIT", &state.tit1, 0, nullptr, p.tit1),
                      nullptr, {},
                      true, ScanGroup::TEMP};  // nonExcludable
    }

    // TIT2
    if (p.hasTit2)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("TIT2", &state.tit2, 0, nullptr, p.tit2),
                      nullptr, {},
                      true, ScanGroup::TEMP};  // nonExcludable
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
                                   decimalsFor(DisplayVarId::OIL_PRESS, p, state), nullptr, p.oilP),
                      true, ScanGroup::TEMP};  // nonExcludable
    }

    // Shock cooling rate (calculated)
    if (p.hasColdRate)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("CLD", &state.coldRate,
                                   decimalsFor(DisplayVarId::COLD, p, state), nullptr, p.coldRate),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    // %HP
    if (p.hasHp)
    {
        pages[n++] = {BottomMode::SINGLE,
                      drawSingleValueUnits,
                      makeValueDef("% HP", &state.hp,
                                   decimalsFor(DisplayVarId::HP, p, state), nullptr, p.hp),
                      nullptr, {},
                      false, ScanGroup::TEMP};
    }

    return n;
}
