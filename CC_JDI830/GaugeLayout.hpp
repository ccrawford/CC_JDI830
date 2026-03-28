#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// GaugeRect — position and size for one gauge's sprite on the display.
// ---------------------------------------------------------------------------
struct GaugeRect {
    int16_t x, y, w, h;
};

// ---------------------------------------------------------------------------
// ArcParams — geometry for an ArcGauge (RPM or MAP), all relative to the
// sprite's own coordinate system.
//
// cx, cy       — arc center within the sprite
// outerR, innerR — radii of the colored arc band
// startAngle, endAngle — sweep in degrees (LovyanGFX convention:
//                         0° = 3 o'clock, clockwise)
// needleLen    — length of the needle from center
// labelY       — y-position for the label text (e.g. "RPM") within the sprite
// ---------------------------------------------------------------------------
struct ArcParams {
    int16_t cx, cy;
    int16_t outerR, innerR;
    float   startAngle, endAngle;
    int16_t needleLen;
    int16_t labelY;
};

// ---------------------------------------------------------------------------
// StaticLine — a horizontal or vertical separator drawn directly to the LCD.
// Drawn as a 2-pixel-wide pair for visibility.
// ---------------------------------------------------------------------------
struct StaticLine {
    int16_t x0, y0, x1, y1;
};
static constexpr int MAX_STATIC_LINES = 4;

// ---------------------------------------------------------------------------
// GaugeLayout — complete positioning data for one screen orientation.
//
// All gauge positions and sizes are stored here.  setupGauges() reads from
// this struct instead of using hardcoded pixel values.
//
// For landscape, a constexpr builder function derives all positions from a
// handful of key boundaries (divider lines, status bar width, etc.).
// Portrait is defined directly since its layout is less regular.
// ---------------------------------------------------------------------------
struct GaugeLayout {
    // Screen dimensions after rotation
    int16_t  screenW, screenH;
    uint8_t  rotation;              // LovyanGFX setRotation() value

    // RPM arc gauge
    GaugeRect rpm;
    ArcParams rpmArc;

    // MAP arc gauge
    GaugeRect map;
    ArcParams mapArc;
    bool      mapInverted;          // true in portrait (arc opens upward)

    // %HP value gauge
    GaugeRect hpPct;
    int16_t   hpGapCenter;          // ValueGauge layout gap parameter

    // EGT/CHT column bars — .w is the default for 6-cyl;
    // DisplayConfig may override it for 4-cyl compact layout.
    GaugeRect egtCht;

    // Right-column HBar region.
    // barY is the top of the first bar, barMaxY is the bottom boundary.
    // buildDefaultConfig() computes how many bars fit:
    //   maxSlots = (barMaxY - barY) / barYSpacing
    // then fills slots from the profile's available variables.
    int16_t barX, barY, barMaxY, barW, barH, barYSpacing;

    // Bottom bar
    GaugeRect bottomBar;

    // Status bar — in portrait this is a horizontal bar at the bottom.
    // In landscape it becomes a narrow vertical bar on the side with
    // stacked characters.
    GaugeRect statusBar;
    bool      statusVertical;       // true → draw stacked vertical text
    int16_t   stepLabelPos, lfLabelPos; // positions along primary axis

    // Debug overlay y-position
    int16_t debugY;

    // Static separator lines drawn directly to the LCD.
    StaticLine staticLines[MAX_STATIC_LINES];
    int        numStaticLines;
};

// ===========================================================================
// LAYOUT_PORTRAIT — 320x480, the original JPI 830 layout.
//
// This layout predates the GaugeLayout system and has an irregular structure
// (RPM above MAP, inverted MAP arc, HBars to the right of arcs, etc.).
// Defined directly with literal values transcribed from the original code.
// ===========================================================================
static constexpr GaugeLayout LAYOUT_PORTRAIT = {
    .screenW = 320, .screenH = 480, .rotation = 0,

    //               x    y    w    h
    .rpm    = {     10,   1, 205,  90 },
    //             cx   cy  outerR innerR  startAngle  endAngle  needleLen  labelY
    .rpmArc = {    75,  85,    75,    70,    182.0f,    358.0f,       55,     25 },

    //               x    y    w    h
    .map         = { 10,  95, 205,  90 },
    //             cx   cy  outerR innerR  startAngle  endAngle  needleLen  labelY
    .mapArc      = { 75,   2,   75,   70,   178.0f,      2.0f,      55,     42 },
    .mapInverted = true,

    .hpPct       = {  2, 185,  90,  20 },
    .hpGapCenter = 35,

    .egtCht = { 5, 205, 310, 165 },

    .barX = 220, .barY = -30, .barMaxY = 380, .barW = 80, .barH = 35, .barYSpacing = 34,

    .bottomBar = { 5, 380, 310, 45 },

    .statusBar     = { 0, 430, 320, 16 },
    .statusVertical = false,
    .stepLabelPos  = 70,  .lfLabelPos = 250,

    .debugY = 465,

    .staticLines = { { 6, 93, 163, 93 } },
    .numStaticLines = 1,
};

// ===========================================================================
// LAYOUT_LANDSCAPE — 480x320, built from key boundaries.
//
// The landscape layout is regular: two divider lines carve the screen into
// three columns (content | HBars | status bar), and a horizontal line
// separates the arc row from the EGT/CHT area.  Almost every gauge rect
// is derived from these boundaries.
//
// To adjust the layout, change the parameters in buildLandscape() below
// and everything else follows.
// ===========================================================================

static constexpr GaugeLayout buildLandscape() {
    // =======================================================================
    // KEY PARAMETERS — change these to reshape the layout
    // =======================================================================
    constexpr int16_t SCREEN_W   = 480;
    constexpr int16_t SCREEN_H   = 320;

    // Status bar: vertical strip on the right edge
    constexpr int16_t statusW    = 20;

    // Vertical divider x: separates content area from HBar column
    constexpr int16_t vLineX     = 338;

    // Horizontal divider y: bottom of arcs, top of EGT/CHT area
    constexpr int16_t hLineY     = 88;

    // EGT/CHT chart height
    constexpr int16_t egtChtH    = 140;

    // Bottom bar height
    constexpr int16_t bottomBarH = 50;

    // Arc geometry (intrinsic to the arc shape, not derivable from boundaries)
    constexpr int16_t arcOuterR  = 60;
    constexpr int16_t arcInnerR  = 55;
    constexpr int16_t arcNeedleLen = 40;
    constexpr float   arcStartAngle = 202.0f;
    constexpr float   arcEndAngle   = 338.0f;
    constexpr int16_t arcLabelY  = 1;       // label above the arc

    // HBar parameters
    constexpr int16_t hbarH        = 28;    // sprite height per bar
    constexpr int16_t hbarYSpacing = 38;    // vertical spacing between bars

    // Padding between divider lines and content
    constexpr int16_t pad = 2;

    // =======================================================================
    // DERIVED VALUES — everything below is computed from the above
    // =======================================================================

    // Content area: left edge to vertical divider
    constexpr int16_t contentX = 0;
    constexpr int16_t contentW = vLineX;                    // 0..vLineX

    // HBar column: between vertical divider and status bar
    constexpr int16_t hbarX    = vLineX + pad;
    constexpr int16_t statusX  = SCREEN_W - statusW;        // right edge
    constexpr int16_t hbarW    = statusX - hbarX - pad;     // fill the gap

    // Arc sprites: two identical arcs, side-by-side, filling content width
    constexpr int16_t arcH     = hLineY;                    // top to divider
    constexpr int16_t arcW     = contentW / 2;              // half of content area each
    constexpr int16_t rpmX     = contentX;
    constexpr int16_t mapX     = contentX + arcW;
    // Arc center: horizontally centered, vertically near the bottom of the sprite
    constexpr int16_t arcCx    = arcW / 2;
    constexpr int16_t arcCy    = arcH - 5;                  // near bottom edge

    // EGT/CHT: below horizontal divider, same width as content area
    constexpr int16_t egtChtX  = contentX;
    constexpr int16_t egtChtY  = hLineY + pad + pad;
    constexpr int16_t egtChtW  = contentW;

    // Bottom bar: below EGT/CHT, same width as content area
    constexpr int16_t bbY      = egtChtY + egtChtH;
    constexpr int16_t bbW      = contentW;

    // %HP: centered between the two arc labels
    constexpr int16_t hpW      = 65;
    constexpr int16_t hpH      = 18;
    constexpr int16_t hpX      = contentW / 2 - hpW / 2;   // centered in content area
    constexpr int16_t hpY      = 5;

    // Status bar label positions: LF at top, STEP at bottom
    constexpr int16_t lfPos    = SCREEN_H / 4;
    constexpr int16_t stepPos  = SCREEN_H * 3 / 4;

    // Debug overlay: below everything
    constexpr int16_t debugY   = SCREEN_H - 13;

    // =======================================================================
    // BUILD THE LAYOUT
    // =======================================================================
    GaugeLayout L = {};

    L.screenW  = SCREEN_W;
    L.screenH  = SCREEN_H;
    L.rotation = 1;

    // RPM arc
    L.rpm    = { rpmX, 0, arcW, arcH };
    L.rpmArc = { arcCx, arcCy, arcOuterR, arcInnerR,
                 arcStartAngle, arcEndAngle, arcNeedleLen, arcLabelY };

    // MAP arc — same geometry, not inverted
    L.map         = { mapX, 0, arcW, arcH };
    L.mapArc      = { arcCx, arcCy, arcOuterR, arcInnerR,
                       arcStartAngle, arcEndAngle, arcNeedleLen, arcLabelY };
    L.mapInverted = false;

    // %HP
    L.hpPct       = { hpX, hpY, hpW, hpH };
    L.hpGapCenter = 28;

    // EGT/CHT
    L.egtCht = { egtChtX, egtChtY, egtChtW, egtChtH };

    // HBars
    L.barX        = hbarX;
    L.barY        = 2 - hbarYSpacing;
    L.barMaxY     = SCREEN_H - pad;
    L.barW        = hbarW;
    L.barH        = hbarH;
    L.barYSpacing = hbarYSpacing;

    // Bottom bar
    L.bottomBar = { contentX, bbY, bbW, bottomBarH };

    // Status bar
    L.statusBar      = { statusX, 0, statusW, SCREEN_H };
    L.statusVertical = true;
    L.stepLabelPos   = stepPos;
    L.lfLabelPos     = lfPos;

    L.debugY = debugY;

    // Static separator lines
    L.staticLines[0] = { contentX, hLineY, vLineX, hLineY };  // horizontal
    L.staticLines[1] = { vLineX,   0,      vLineX, SCREEN_H };// vertical: HBar edge
    // L.staticLines[2] = { statusX - pad, 0, statusX - pad, SCREEN_H }; // vertical: status edge
    L.numStaticLines = 2;

    return L;
}

static constexpr GaugeLayout LAYOUT_LANDSCAPE = buildLandscape();
