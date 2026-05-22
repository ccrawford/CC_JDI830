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
// labelY       — y-position for the label text (e.g. "RPM") within the sprite
// ---------------------------------------------------------------------------
struct ArcParams {
    int16_t cx, cy;
    int16_t outerR, innerR;
    float   startAngle, endAngle;
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
    GaugeRect rpmHpCutout;          // transparent rect in sprite for %HP overlay (w=0 = none)

    // MAP arc gauge
    GaugeRect map;
    ArcParams mapArc;
    bool      mapInverted;          // true in portrait (arc opens upward)
    GaugeRect mapHpCutout;          // transparent rect in sprite for %HP overlay (w=0 = none)

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
// Built from a handful of tunable constants. Tweak the values in
// buildPortrait() and all derived positions follow automatically.
//
// Layout structure (top to bottom):
//   [ RPM arc ] [ HBars ]   ← arc row 1
//   [ MAP arc ] [ HBars ]   ← arc row 2 (MAP is inverted, opens upward)
//        %HP overlay between RPM and MAP arcs
//   [ EGT/CHT cluster      ]
//   [ Bottom bar (centered)]
//   [ Status bar — full width, no border ]
// ===========================================================================

static constexpr GaugeLayout buildPortrait() {
    // =======================================================================
    // KEY PARAMETERS — change these to reshape the layout
    // =======================================================================
    constexpr int16_t SCREEN_W   = 320;
    constexpr int16_t SCREEN_H   = 480;

    // Screen border (status bar ignores these — it's always full-width,
    // edge-to-edge at the very bottom).
    constexpr int16_t borderH    = 4;   // left/right margin
    constexpr int16_t borderV    = 4;   // top margin (bottom handled by status bar)

    // Status bar — fixed height, always pinned to the bottom edge
    constexpr int16_t statusH    = 16;

    // Arc sprites: two identical arcs stacked vertically (RPM on top, MAP below).
    // Same width, same radii. arcX is the left edge (independent of borderH so
    // arcs can shift without affecting the EGT/CHT or bottom-bar alignment).
    constexpr int16_t arcX       = -6;
    constexpr int16_t arcW       = 205;
    constexpr int16_t arcH       = 90;
    constexpr int16_t arcOuterR  = 75;
    constexpr int16_t arcInnerR  = 70;
    // RPM arc opens downward (standard); MAP arc is inverted (opens upward).
    constexpr float   rpmStartAngle = 182.0f;
    constexpr float   rpmEndAngle   = 358.0f;
    constexpr int16_t rpmLabelY     = 25;
    constexpr float   mapStartAngle = 178.0f;
    constexpr float   mapEndAngle   = 2.0f;
    constexpr int16_t mapLabelY     = 42;

    // HBar column (top-right). Tuned independently of arc width.
    constexpr int16_t hbarX        = 220;   // left edge of HBar column
    constexpr int16_t hbarW        = 80;    // width of HBar sprites
    constexpr int16_t hbarH        = 35;    // sprite height per bar
    constexpr int16_t hbarYSpacing = 33;    // vertical spacing between bars
    // First bar's top edge target — bars are positioned at
    //   barY + (i+1) * barYSpacing, so barY = hbarTopY - barYSpacing.
    constexpr int16_t hbarTopY     = 3;

    // EGT/CHT cluster — Y and H tuned directly; X/W derive from screen border.
    constexpr int16_t egtChtY      = 205;
    constexpr int16_t egtChtH      = 165;

    // Bottom bar height. Y is derived (centered between EGT/CHT bottom and
    // top of status bar).
    constexpr int16_t bottomBarH   = 45;

    // %HP value gauge — upper-left corner and size. Overlaps the lower-left
    // of the MAP arc sprite; the cutout is configured below.
    constexpr int16_t hpX          = 2;
    constexpr int16_t hpY          = 175;
    constexpr int16_t hpW          = 80;
    constexpr int16_t hpH          = 20;
    constexpr int16_t hpGapCenter  = 35;

    // Status bar label positions (along the bar)
    constexpr int16_t stepLabelPos = 70;
    constexpr int16_t lfLabelPos   = 250;

    // =======================================================================
    // DERIVED VALUES — computed from the above
    // =======================================================================

    // Arcs: stacked vertically. arcX is set in the tunables block above.
    constexpr int16_t rpmY     = borderV;             // top of RPM sprite
    constexpr int16_t mapY     = rpmY + arcH;         // MAP directly below RPM
    constexpr int16_t arcCx    = arcW / 2;            // both arcs centered horizontally in sprite
    // RPM arc: center near the bottom of its sprite (arc opens downward)
    constexpr int16_t rpmCy    = arcH - 5;
    // MAP arc: center near the top of its sprite (inverted, opens upward)
    constexpr int16_t mapCy    = 2;

    // EGT/CHT: full usable width
    constexpr int16_t egtChtX  = borderH;
    constexpr int16_t egtChtW  = SCREEN_W - 2 * borderH;

    // Status bar: full width at the very bottom, ignores borders
    constexpr int16_t statusY  = SCREEN_H - statusH;

    // Bottom bar: centered vertically in the gap between EGT/CHT and status bar
    constexpr int16_t bbGapTop = egtChtY + egtChtH;
    constexpr int16_t bbGapH   = statusY - bbGapTop;
    constexpr int16_t bbY      = bbGapTop + (bbGapH - bottomBarH) / 2;
    constexpr int16_t bbX      = borderH;
    constexpr int16_t bbW      = SCREEN_W - 2 * borderH;

    // HBar setup: convert "top of first bar" to the barY sentinel that
    // setupRightBars() expects: bar i is at barY + (i+1) * spacing.
    constexpr int16_t hbarBarY = hbarTopY - hbarYSpacing;
    // Cap HBars at the top of EGT/CHT so they don't bleed into that area.
    constexpr int16_t hbarMaxY = egtChtY;

    // Debug overlay: just above the status bar
    constexpr int16_t debugY   = statusY - 15;

    // %HP cutout: the overlay sits on top of the MAP arc sprite (lower-left
    // corner, since the MAP arc is inverted and that corner is empty space).
    // We carve a transparent rect in the MAP sprite the size of the overlay,
    // clipped to the sprite's bounds. RPM sprite needs no cutout in portrait.
    constexpr int16_t mapCutX  = (hpX > arcX)            ? (hpX - arcX)        : 0;
    constexpr int16_t mapCutY  = (hpY > mapY)            ? (hpY - mapY)        : 0;
    constexpr int16_t mapCutXEnd = (hpX + hpW < arcX + arcW) ? (hpX + hpW - arcX) : arcW;
    constexpr int16_t mapCutYEnd = (hpY + hpH < mapY + arcH) ? (hpY + hpH - mapY) : arcH;
    constexpr int16_t mapCutW  = mapCutXEnd - mapCutX;
    constexpr int16_t mapCutH  = mapCutYEnd - mapCutY;

    // =======================================================================
    // BUILD THE LAYOUT
    // =======================================================================
    GaugeLayout L = {};

    L.screenW  = SCREEN_W;
    L.screenH  = SCREEN_H;
    L.rotation = 0;

    // RPM arc (top) — no overlay sits on it in portrait
    L.rpm         = { arcX, rpmY, arcW, arcH };
    L.rpmArc      = { arcCx, rpmCy, arcOuterR, arcInnerR,
                      rpmStartAngle, rpmEndAngle, rpmLabelY };
    L.rpmHpCutout = { 0, 0, 0, 0 };

    // MAP arc (below RPM, inverted) — %HP overlay lives in its lower-left
    L.map         = { arcX, mapY, arcW, arcH };
    L.mapArc      = { arcCx, mapCy, arcOuterR, arcInnerR,
                       mapStartAngle, mapEndAngle, mapLabelY };
    L.mapInverted = true;
    L.mapHpCutout = { mapCutX, mapCutY, mapCutW, mapCutH };

    // %HP overlay
    L.hpPct       = { hpX, hpY, hpW, hpH };
    L.hpGapCenter = hpGapCenter;

    // EGT/CHT
    L.egtCht = { egtChtX, egtChtY, egtChtW, egtChtH };

    // HBars
    L.barX        = hbarX;
    L.barY        = hbarBarY;
    L.barMaxY     = hbarMaxY;
    L.barW        = hbarW;
    L.barH        = hbarH;
    L.barYSpacing = hbarYSpacing;

    // Bottom bar
    L.bottomBar = { bbX, bbY, bbW, bottomBarH };

    // Status bar (full width, no border)
    L.statusBar      = { 0, statusY, SCREEN_W, statusH };
    L.statusVertical = false;
    L.stepLabelPos   = stepLabelPos;
    L.lfLabelPos     = lfLabelPos;

    L.debugY = debugY;

    // No static separator lines in portrait
    L.numStaticLines = 0;

    return L;
}

static constexpr GaugeLayout LAYOUT_PORTRAIT = buildPortrait();

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
    constexpr int16_t bbY      = egtChtY + egtChtH + 15;  // Should probably center this in the available space.
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

    // RPM arc — %HP overlay sits centered at the top between the two arcs,
    // so each arc carves its inner top corner transparent.
    L.rpm         = { rpmX, 0, arcW, arcH };
    L.rpmArc      = { arcCx, arcCy, arcOuterR, arcInnerR,
                      arcStartAngle, arcEndAngle, arcLabelY };
    L.rpmHpCutout = { arcW - 40, 0, 40, 25 };  // top-right corner

    // MAP arc — same geometry, not inverted
    L.map         = { mapX, 0, arcW, arcH };
    L.mapArc      = { arcCx, arcCy, arcOuterR, arcInnerR,
                       arcStartAngle, arcEndAngle, arcLabelY };
    L.mapInverted = false;
    L.mapHpCutout = { 0, 0, 40, 25 };          // top-left corner

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
