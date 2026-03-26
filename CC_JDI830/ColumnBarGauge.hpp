#pragma once
#include "JPIGauges.hpp"

// ---------------------------------------------------------------------------
// ColumnBarGauge — segmented vertical bar chart for EGT/CHT per cylinder,
// with optional TIT column(s).
//
// This gauge is a composite: it displays N cylinders, each with an EGT bar
// and a CHT bar. Optionally, 1-2 TIT columns appear after the cylinder
// columns as single bars with their own color and right-side scale.
//
// Left side:  color scale bar (green/red based on CHT redline) + tick labels
// Right side: TIT scale labels (when TIT columns are present)
//
// The bars are made of discrete stacked blocks rather than continuous fills,
// matching the JPI 830 look.
//
// This gauge doesn't use the base class _valuePtr / _minVal / _maxVal
// since it manages multiple values with dual scales. It overrides update()
// to do its own dirty-checking across all cylinders.
// ---------------------------------------------------------------------------

static constexpr int MAX_CYLINDERS = 8;
static constexpr int MAX_TIT = 2;

class ColumnBarGauge : public Gauge
{
private:
    // --- Per-cylinder data ---
    int _numCylinders;

    const float *_egtPtrs[MAX_CYLINDERS]; // pointers into curState.egt[]
    const float *_chtPtrs[MAX_CYLINDERS]; // pointers into curState.cht[]
    float _prevEgt[MAX_CYLINDERS];
    float _prevCht[MAX_CYLINDERS];

    // --- TIT columns (optional, drawn after cylinder columns) ---
    int _numTit;                          // 0, 1, or 2
    const float *_titPtrs[MAX_TIT];
    float _prevTit[MAX_TIT];
    const char *_titLabels[MAX_TIT];      // "T", "T1", "T2", etc.
    float _titMin, _titMax;
    float _titRedline;
    int _titColor;

    // --- Dual scale ranges ---
    float _egtMin, _egtMax; // e.g. 850 - 1700 (EGT bar fill scale)
    float _chtMin, _chtMax; // e.g. 200 - 600  (CHT bar fill scale, also left scale)

    // --- Red limit lines (drawn as horizontal lines across the chart) ---
    float _egtRedline; // e.g. 1650
    float _chtRedline; // e.g. 450

    // --- Layout (computed by computeLayout() in init) ---
    int16_t _chartLeft;   // left edge of first column (after scale area)
    int16_t _chartTop;    // top of bar area (below value labels if shown)
    int16_t _chartBottom; // bottom of bar area (above cylinder labels)

    int16_t _barWidth;    // width of each EGT or CHT bar (computed)
    int16_t _barHeight;   // height of each discrete block segment (computed)
    int16_t _colGap;      // gap between cylinder columns (computed)

    // Configurable margins — set these before init() if defaults don't work
    int16_t _scaleWidth;      // pixels reserved on left for scale bar + labels
    int16_t _rightScaleWidth; // pixels reserved on right for TIT scale (0 if no TIT)
    int16_t _labelAreaHeight; // pixels reserved on bottom for cylinder numbers
    int16_t _valueAreaHeight; // pixels reserved on top for EGT/CHT value text (0 if not shown)

    // Left scale bar geometry (within _scaleWidth)
    static constexpr int16_t SCALE_BAR_WIDTH = 6;  // width of the colored vertical bar
    static constexpr int16_t SCALE_BAR_GAP = 3;    // gap between scale bar and first column
    static constexpr int16_t TICK_LEN = 3;          // tick mark length

    static constexpr int16_t BLOCK_GAP = 2; // vertical gap between block segments
    static constexpr int16_t BAR_GAP = 2;   // horizontal gap between EGT and CHT bar in a pair
    static constexpr int16_t COL_GAP = 8;   // fixed gap between cylinder columns

    int _selectedCylinder; // highlighted cylinder (boxed number), -1 for none

    const uint8_t *_scaleFont;
    const uint8_t *_labelFont;

    int _egtColor;
    int _chtColor;
    int _egtPeakColor; // color for the peak EGT cylinder

    const uint8_t *_valueFont; // font for numeric values drawn above the bars
    bool _showValues;          // whether to draw EGT/CHT numbers above the bars

    // --- Normalize mode (EGT only) ---
    // When active, all EGT bars are drawn at half chart height (the "baseline"),
    // and deviations from the snapshot are shown as segment changes.
    // Each segment = 10°F of change. CHT/TIT are unaffected.
    bool _normalizeMode;
    float _nrmBaseline[MAX_CYLINDERS]; // EGT snapshot taken when normalize is activated

protected:
    void drawGauge() override
    {
        drawLeftScale();
        if (_showValues)
            drawValueLabels();
        drawScaleLabels();
        drawRedlines();
        drawColumns();
        drawTitColumns();
        drawCylinderLabels();
        if (_numTit > 0)
            drawRightScale();

        // NRM indicator has moved to the StatusBar center indicator.
        // ColumnBarGauge no longer draws it.
    }

private:
    // Map an EGT value to a Y pixel coordinate within the chart area.
    // Note: Y is inverted — higher values = lower Y coordinate (top of screen).
    int16_t egtToY(float val)
    {
        return mapValue(val, _egtMin, _egtMax, _chartBottom, _chartTop);
    }

    int16_t chtToY(float val)
    {
        return mapValue(val, _chtMin, _chtMax, _chartBottom, _chartTop);
    }

    int16_t titToY(float val)
    {
        return mapValue(val, _titMin, _titMax, _chartBottom, _chartTop);
    }

    // Distance in pixels from one cylinder column's left edge to the next.
    // Each column is: [egtBar][BAR_GAP][chtBar], then _colGap to the next.
    int16_t colStride() const
    {
        return _barWidth * 2 + BAR_GAP + _colGap;
    }

    // X position of the TIT column's left edge — one colStride() after the
    // last cylinder column, so TIT sits at the same spacing as cylinders.
    int16_t titColX() const
    {
        return _chartLeft + _numCylinders * colStride();
    }

    // --- Left-side color scale bar ---
    // Draws a thin vertical bar colored green (normal) and red (above redline),
    // with tick marks and numeric labels at key CHT values.
    void drawLeftScale()
    {
        // The color bar sits just left of _chartLeft
        int16_t barX = _chartLeft - SCALE_BAR_GAP - SCALE_BAR_WIDTH;

        // Green section: from _chtMin up to redline (or _chtMax if no redline)
        float greenTop = (_chtRedline > 0) ? _chtRedline : _chtMax;
        int16_t greenTopY = chtToY(greenTop);
        int16_t greenBotY = chtToY(_chtMin);
        _sprite.fillRect(barX, greenTopY, SCALE_BAR_WIDTH, greenBotY - greenTopY, TFT_GREEN);

        // Red section: a short zone above the redline (not the full range).
        // The real JPI shows just a small red band, not all the way to the top.
        if (_chtRedline > 0)
        {
            // Extend the red zone ~15% of the CHT range above the redline
            float redExtent = (_chtMax - _chtMin) * 0.15f;
            float redTop = _chtRedline + redExtent;
            int16_t redTopY = chtToY(redTop);
            int16_t redBotY = chtToY(_chtRedline);
            _sprite.fillRect(barX, redTopY, SCALE_BAR_WIDTH, redBotY - redTopY, TFT_RED);
        }

        // Tick marks and labels
        if (_scaleFont)
            _sprite.loadFont(_scaleFont);

        int16_t tickX = barX - 1; // tick marks extend left from the bar

        // Helper lambda to draw a tick + label at a given CHT value.
        // C++ note: a lambda is an inline, unnamed function. The [&] means
        // "capture everything by reference" — so it can access _sprite,
        // _chartLeft, etc. without passing them as parameters. Lambdas are
        // great for small helpers that are only used in one place.
        auto drawTick = [&](float val, int color) {
            int16_t y = chtToY(val);
            _sprite.drawFastHLine(tickX - TICK_LEN, y, TICK_LEN, color);
            _sprite.setTextDatum(MR_DATUM);
            _sprite.setTextColor(color);
            _sprite.drawNumber((int)val, tickX - TICK_LEN - 2, y);
        };

        // Min value at bottom
        drawTick(_chtMin, TFT_WHITE);

        // Redline value
        if (_chtRedline > 0)
            drawTick(_chtRedline, TFT_RED);

        // A round-number tick between min and redline (e.g. 400 for range 200-450)
        if (_chtRedline > 0)
        {
            int midTick = ((int)(_chtRedline / 100)) * 100;
            if (midTick > (int)_chtMin && midTick < (int)_chtRedline)
                drawTick((float)midTick, TFT_WHITE);
        }

        // Units label at bottom
        _sprite.setTextDatum(TR_DATUM);
        _sprite.setTextColor(TFT_WHITE);
        _sprite.drawString("\xc2\xb0""F", barX + SCALE_BAR_WIDTH, _chartBottom + 3);
    }

    void drawColumns()
    {
        int dispColor = TFT_BLACK;

        for (int cyl = 0; cyl < _numCylinders; cyl++)
        {
            int16_t colX = _chartLeft + cyl * colStride();

            // EGT bar (left bar in each pair)
            float egtVal = _egtPtrs[cyl] ? *_egtPtrs[cyl] : 0;

            if (_normalizeMode)
            {
                // Normalize view: build a synthetic scale where the baseline
                // maps to half chart height and each segment = 10°F deviation.
                //
                // How it works:
                //   segStep      = pixels per segment (block height + gap)
                //   totalSegs    = how many segments fit in the chart height
                //   fullScale    = totalSegs * 10°F (full scale in degrees)
                //   midValue     = fullScale / 2  (baseline sits at half height)
                //   syntheticVal = midValue + (currentEgt - baseline)
                //
                // We then call drawSegmentedBar() with [0, fullScale] as the
                // scale, so mapValue() maps midValue to the chart midpoint.
                int16_t segStep   = _barHeight + BLOCK_GAP;
                int     totalSegs = (_chartBottom - _chartTop) / segStep;
                float   fullScale = totalSegs * 10.0f;
                float   midValue  = fullScale / 2.0f;

                float deviation = egtVal - _nrmBaseline[cyl];
                float synVal    = midValue + deviation;

                // Clamp to scale bounds so drawSegmentedBar doesn't overflow
                if (synVal < 0.0f)      synVal = 0.0f;
                if (synVal > fullScale)  synVal = fullScale;

                // No redline coloring in normalize mode (deviation display)
                drawSegmentedBar(colX, synVal, 0.0f, fullScale, _egtColor);
            }
            else
            {
                dispColor = egtVal >= _egtRedline ? TFT_RED : _egtColor;
                drawSegmentedBar(colX, egtVal, _egtMin, _egtMax, dispColor);
            }

            // CHT bar (right bar in each pair)
            float chtVal = _chtPtrs[cyl] ? *_chtPtrs[cyl] : 0;
            dispColor = chtVal >= _chtRedline ? TFT_RED : _chtColor;
            drawSegmentedBar(colX + _barWidth + BAR_GAP, chtVal, _chtMin, _chtMax, dispColor);

            // Dashed center line between bars (the dotted vertical line in the JPI)
            int16_t cx = colX + _barWidth + BAR_GAP / 2;
            for (int16_t y = _chartTop; y <= _chartBottom; y += 6)
            {
                _sprite.drawPixel(cx, y, TFT_LIGHTGRAY);
                _sprite.drawPixel(cx, y + 1, TFT_LIGHTGRAY);
            }
        }
    }

    // Draw TIT column — laid out like a cylinder pair (same stride/spacing).
    // With 1 TIT: single bar on the left, center line in the middle.
    // With 2 TITs: T1 left bar, T2 right bar, center line between — just
    // like EGT/CHT in a cylinder column.
    void drawTitColumns()
    {
        if (_numTit == 0) return;

        int16_t colX = titColX();

        // TIT1 — left bar (same position as EGT in a cylinder column)
        float val1 = _titPtrs[0] ? *_titPtrs[0] : 0;
        int color1 = (_titRedline > 0 && val1 >= _titRedline) ? TFT_RED : _titColor;
        drawSegmentedBar(colX, val1, _titMin, _titMax, color1);

        // TIT2 — right bar (same position as CHT in a cylinder column)
        if (_numTit > 1)
        {
            float val2 = _titPtrs[1] ? *_titPtrs[1] : 0;
            int color2 = (_titRedline > 0 && val2 >= _titRedline) ? TFT_RED : _titColor;
            drawSegmentedBar(colX + _barWidth + BAR_GAP, val2, _titMin, _titMax, color2);
        }

        // Dashed center line (between T1 and T2, or center of single T)
        int16_t cx = colX + _barWidth + BAR_GAP / 2;
        for (int16_t y = _chartTop; y <= _chartBottom; y += 6)
        {
            _sprite.drawPixel(cx, y, TFT_LIGHTGRAY);
            _sprite.drawPixel(cx, y + 1, TFT_LIGHTGRAY);
        }

        // TIT redline marker across the TIT column
        if (_titRedline > 0)
        {
            int16_t ry = titToY(_titRedline);
            int16_t lineW = (_numTit > 1) ? (2 * _barWidth + BAR_GAP + 4) : (_barWidth + 4);
            _sprite.drawFastHLine(colX - 2, ry, lineW, TFT_RED);
        }
    }

    // Draw a single segmented (blocky) vertical bar.
    void drawSegmentedBar(int16_t x, float value, float scaleMin, float scaleMax,
                          int color)
    {
        int16_t barTop = mapValue(value, scaleMin, scaleMax, _chartBottom, _chartTop);

        // Draw blocks from bottom up to the value
        for (int16_t y = _chartBottom; y >= barTop; y -= (_barHeight + BLOCK_GAP))
        {
            int16_t blockH = _barHeight;
            // Don't overshoot the top
            if (y - blockH < barTop)
                blockH = y - barTop;
            if (blockH <= 0)
                break;

            _sprite.fillRect(x, y - blockH, _barWidth, blockH, color);
        }
    }

    void drawRedlines()
    {
        // CHT redline — spans across all cylinder columns only (not TIT)
        if (_chtRedline > 0)
        {
            int16_t y = chtToY(_chtRedline);
            int16_t lineWidth = (_numCylinders - 1) * colStride() + 2 * _barWidth + BAR_GAP + 4;
            _sprite.drawFastHLine(_chartLeft - 2, y, lineWidth, TFT_RED);
        }
    }

    void drawScaleLabels()
    {
        // Left scale labels are now drawn by drawLeftScale().
        // This method is kept for any future additional labels.
    }

    // --- Right-side TIT scale ---
    // Drawn when TIT columns are present. Shows min value and redline
    // on the right edge, matching the reference JPI layout.
    void drawRightScale()
    {
        if (_scaleFont)
            _sprite.loadFont(_scaleFont);

        // Right scale position: after the TIT column (same width as a cylinder column)
        int16_t scaleX = titColX() + 2 * _barWidth + BAR_GAP + SCALE_BAR_GAP;

        _sprite.setTextDatum(ML_DATUM);

        // Min value at bottom
        _sprite.setTextColor(TFT_WHITE);
        _sprite.drawNumber((int)_titMin, scaleX, _chartBottom);

        // Redline value
        if (_titRedline > 0)
        {
            int16_t ry = titToY(_titRedline);
            _sprite.setTextColor(TFT_RED);
            _sprite.drawNumber((int)_titRedline, scaleX, ry);
        }
    }

    void drawCylinderLabels()
    {
        if (_labelFont)
            _sprite.loadFont(_labelFont);
        _sprite.setTextDatum(TC_DATUM);

        // Cylinder number labels (1-based)
        for (int cyl = 0; cyl < _numCylinders; cyl++)
        {
            int16_t cx = _chartLeft + cyl * colStride() + _barWidth + BAR_GAP / 2;
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", cyl + 1);

            if (cyl == _selectedCylinder)
            {
                // Draw boxed number for selected cylinder
                int16_t tw = _sprite.textWidth(buf);
                int16_t th = _sprite.fontHeight();
                int16_t bx = cx - th / 2;
                int16_t by = _chartBottom + 2;
                _sprite.drawRect(bx, by, th + 2, th + 2, TFT_WHITE);
                _sprite.setTextColor(TFT_WHITE);
                _sprite.drawString(buf, cx, by + 1);
            }
            else
            {
                _sprite.setTextColor(TFT_WHITE);
                _sprite.drawString(buf, cx, _chartBottom + 2);
            }
        }

        // TIT column label — centered on the dashed center line
        if (_numTit > 0)
        {
            int16_t cx = titColX() + _barWidth + BAR_GAP / 2;
            _sprite.setTextColor(TFT_WHITE);
            _sprite.drawString("T", cx, _chartBottom + 2);
        }
    }

    // Draw EGT and CHT numeric values above the bar chart area.
    // Each value is centered on its cylinder's dashed center line,
    // colored to match the bar (normal color, or red if above redline).
    void drawValueLabels()
    {
        if (_valueFont)
            _sprite.loadFont(_valueFont);
        _sprite.setTextDatum(BC_DATUM); // bottom-center

        int16_t fontH = _sprite.fontHeight();
        // Two rows stacked above _chartTop: CHT row is just above the chart,
        // EGT row is above that.
        int16_t chtRowY = _chartTop - 2;
        int16_t egtRowY = chtRowY - fontH;

        for (int cyl = 0; cyl < _numCylinders; cyl++)
        {
            // Center X matches the dashed line between EGT/CHT bar pair
            int16_t cx = _chartLeft + cyl * colStride()
                         + _barWidth + BAR_GAP / 2;

            // EGT value
            float egtVal = _egtPtrs[cyl] ? *_egtPtrs[cyl] : 0;
            int egtClr = egtVal >= _egtRedline ? TFT_RED : _egtColor;
            _sprite.setTextColor(egtClr);
            _sprite.drawNumber((int)egtVal, cx, egtRowY);

            // CHT value
            float chtVal = _chtPtrs[cyl] ? *_chtPtrs[cyl] : 0;
            int chtClr = chtVal >= _chtRedline ? TFT_RED : _chtColor;
            _sprite.setTextColor(chtClr);
            _sprite.drawNumber((int)chtVal, cx, chtRowY);
        }

        // TIT values — centered on the TIT column's dashed center line.
        // T1 on the top (EGT) row, T2 on the bottom (CHT) row.
        if (_numTit > 0)
        {
            int16_t cx = titColX() + _barWidth + BAR_GAP / 2;

            float val1 = _titPtrs[0] ? *_titPtrs[0] : 0;
            int clr1 = (_titRedline > 0 && val1 >= _titRedline) ? TFT_RED : _titColor;
            _sprite.setTextColor(clr1);
            _sprite.drawNumber((int)val1, cx, egtRowY);

            if (_numTit > 1)
            {
                float val2 = _titPtrs[1] ? *_titPtrs[1] : 0;
                int clr2 = (_titRedline > 0 && val2 >= _titRedline) ? TFT_RED : _titColor;
                _sprite.setTextColor(clr2);
                _sprite.drawNumber((int)val2, cx, chtRowY);
            }
        }
    }

public:
    ColumnBarGauge(LGFX *lcd)
        : Gauge(lcd), _numCylinders(6),
          _numTit(0), _titMin(0), _titMax(0), _titRedline(0), _titColor(TFT_CYAN),
          _egtMin(200), _egtMax(450), _chtMin(200), _chtMax(850),
          _barWidth(0), _barHeight(0), _colGap(0),
          _egtRedline(0), _chtRedline(0),
          _chartLeft(0), _chartTop(0), _chartBottom(0),
          _scaleWidth(35), _rightScaleWidth(0), _labelAreaHeight(24), _valueAreaHeight(0),
          _selectedCylinder(-1), _scaleFont(nullptr), _labelFont(nullptr),
          _egtColor(TFT_BLUE), _chtColor(TFT_CYAN), _egtPeakColor(TFT_RED),
          _valueFont(nullptr), _showValues(false),
          _normalizeMode(false)
    {
        for (int i = 0; i < MAX_CYLINDERS; i++)
        {
            _egtPtrs[i] = nullptr;
            _chtPtrs[i] = nullptr;
            _prevEgt[i] = -99999.0f;
            _prevCht[i] = -99999.0f;
            _nrmBaseline[i] = 0.0f;
        }
        for (int i = 0; i < MAX_TIT; i++)
        {
            _titPtrs[i] = nullptr;
            _prevTit[i] = -99999.0f;
            _titLabels[i] = "T";
        }
    }

    // --- Configuration (call all of these before init()) ---

    void setNumCylinders(int n) { _numCylinders = (n <= MAX_CYLINDERS) ? n : MAX_CYLINDERS; }
    void setEgtRange(float min, float max)
    {
        _egtMin = min;
        _egtMax = max;
    }
    void setChtRange(float min, float max)
    {
        _chtMin = min;
        _chtMax = max;
    }
    void setEgtRedline(float val) { _egtRedline = val; }
    void setChtRedline(float val) { _chtRedline = val; }
    void setSelectedCylinder(int cyl)
    {
        _selectedCylinder = cyl;
        _dirty = true;
    }

    // --- EGT Normalize mode ---
    // When switching to normalize, snapshot all current EGT values as the
    // baseline. All EGT bars will then display at half chart height, with
    // deviations shown as segment changes (1 segment = 10°F).
    // CHT, TIT, and numeric value labels are unaffected.
    void setNormalize(bool on)
    {
        if (on && !_normalizeMode)
        {
            // Capture baseline — snapshot current EGT values
            for (int i = 0; i < _numCylinders; i++)
                _nrmBaseline[i] = _egtPtrs[i] ? *_egtPtrs[i] : 0;
        }
        _normalizeMode = on;
        _dirty = true;
    }

    bool isNormalized() const { return _normalizeMode; }

    // --- TIT configuration ---
    // Add a TIT column. Call once for TIT1, twice for TIT1+TIT2.
    // The label appears below the bar (e.g. "T" or "T1").
    void addTit(const float *ptr, const char *label = "T")
    {
        if (_numTit < MAX_TIT)
        {
            _titPtrs[_numTit] = ptr;
            _titLabels[_numTit] = label;
            _numTit++;
            _rightScaleWidth = 35; // reserve space for right scale
        }
    }
    void setTitRange(float min, float max) { _titMin = min; _titMax = max; }
    void setTitRedline(float val) { _titRedline = val; }
    void setTitColor(int c) { _titColor = c; }

    // Init override — creates the sprite then computes all bar layout
    // from the sprite size and cylinder count. Call setNumCylinders(),
    // addTit(), setValueFont(), setScaleWidth(), etc. BEFORE calling init().
    //
    // C++ note: this "hides" the base class init() rather than overriding it
    // (init isn't virtual). That's fine here — ColumnBarGauge always calls
    // its own init(), never through a base Gauge pointer.
    bool init(int16_t w, int16_t h)
    {
        bool ok = Gauge::init(w, h);
        if (ok) computeLayout();
        return ok;
    }

    // Optional: override the default left margin reserved for scale bar + labels.
    // Call before init().
    void setScaleWidth(int16_t w) { _scaleWidth = w; }

    // Optional: override the default bottom margin reserved for cylinder labels.
    // Call before init().
    void setLabelAreaHeight(int16_t h) { _labelAreaHeight = h; }

    // Compute all layout geometry from the sprite dimensions, cylinder count,
    // and TIT column count. Called automatically at the end of init().
    void computeLayout()
    {
        // Vertical layout: [value labels] [chart bars] [cylinder labels]
        _chartTop = _valueAreaHeight;
        _chartBottom = _h - _labelAreaHeight;

        // Horizontal layout:
        //   [left scale] [cylinder columns] [TIT columns] [right scale]
        _chartLeft = _scaleWidth;
        int16_t availableWidth = _w - _scaleWidth - _rightScaleWidth;

        // TIT occupies one column at the same stride as a cylinder column,
        // so total columns = N_cyl + (hasTit ? 1 : 0).
        // Each column = 2*barWidth + BAR_GAP, with COL_GAP between columns.
        //   availW = N_cols * (2*barWidth + BAR_GAP) + (N_cols-1) * COL_GAP
        // Solve for barWidth:
        int totalCols = _numCylinders + (_numTit > 0 ? 1 : 0);
        int16_t fixedSpace = totalCols * BAR_GAP + (totalCols - 1) * COL_GAP;
        _barWidth = (availableWidth - fixedSpace) / (2 * totalCols);
        if (_barWidth < 3) _barWidth = 3;
        _colGap = COL_GAP;

        // Block height: scale so roughly 20-30 blocks fit the chart height.
        int16_t chartHeight = _chartBottom - _chartTop;
        _barHeight = chartHeight / 25;
        if (_barHeight < 2) _barHeight = 2;
        if (_barHeight > 6) _barHeight = 6;
    }

    void setScaleFont(const uint8_t *font) { _scaleFont = font; }
    void setLabelFont(const uint8_t *font) { _labelFont = font; }
    void setEgtColor(int c) { _egtColor = c; }
    void setChtColor(int c) { _chtColor = c; }

    // Set the font for numeric EGT/CHT values displayed above the bars.
    // Also reserves vertical space for two rows of text. Call before init().
    // If 30px isn't right for your font, call setValueAreaHeight() after this.
    void setValueFont(const uint8_t *font)
    {
        _valueFont = font;
        _showValues = true;
        _valueAreaHeight = 36; // default for two rows of small text
    }

    // Override the top margin reserved for value labels (default 30 when values shown).
    // Call before init().
    void setValueAreaHeight(int16_t h) { _valueAreaHeight = h; }

    void setEgtPtr(int cyl, const float *ptr)
    {
        if (cyl < MAX_CYLINDERS)
            _egtPtrs[cyl] = ptr;
    }
    void setChtPtr(int cyl, const float *ptr)
    {
        if (cyl < MAX_CYLINDERS)
            _chtPtrs[cyl] = ptr;
    }

    // Override update() to check all cylinder values for changes.
    // C++ note: `override` tells the compiler "I intend to override a virtual
    // function from the base class." If you typo the name or get the signature
    // wrong, the compiler gives an error instead of silently creating a new
    // function that never gets called. Always use it.
    void update() override
    {
        // Check all EGT/CHT values for changes
        for (int i = 0; i < _numCylinders; i++)
        {
            if (_egtPtrs[i] && *_egtPtrs[i] != _prevEgt[i])
            {
                _prevEgt[i] = *_egtPtrs[i];
                _dirty = true;
            }
            if (_chtPtrs[i] && *_chtPtrs[i] != _prevCht[i])
            {
                _prevCht[i] = *_chtPtrs[i];
                _dirty = true;
            }
        }

        // Check TIT values for changes
        for (int i = 0; i < _numTit; i++)
        {
            if (_titPtrs[i] && *_titPtrs[i] != _prevTit[i])
            {
                _prevTit[i] = *_titPtrs[i];
                _dirty = true;
            }
        }

        if (_dirty)
        {
            _sprite.fillSprite(TFT_BLACK);
            if(false) _sprite.drawRect(0,0,_w,_h,TFT_PINK);
            drawGauge();
            _sprite.pushSprite(_x, _y);
            _dirty = false;
        }
    }
};
