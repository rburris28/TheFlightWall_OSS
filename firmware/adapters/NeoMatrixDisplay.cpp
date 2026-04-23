/*
Purpose: Render flight info on a WS2812B NeoPixel matrix via FastLED_NeoMatrix.
Responsibilities:
- Initialize LED matrix based on HardwareConfiguration and user display settings.
- Render a bordered flight card with optional local map inset and a minimal loading screen.
- Cycle through multiple flights at a configurable interval.
Inputs: FlightInfo list; UserConfiguration (colors/brightness), TimingConfiguration (cycle),
        HardwareConfiguration (dimensions/pin/tiling).
Outputs: Visual output to LED matrix using FastLED.
*/
#include "adapters/NeoMatrixDisplay.h"

#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <math.h>
#include "config/UserConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "config/TimingConfiguration.h"

NeoMatrixDisplay::NeoMatrixDisplay() {}

NeoMatrixDisplay::~NeoMatrixDisplay()
{
    if (_leds)
    {
        delete[] _leds;
        _leds = nullptr;
    }
    if (_matrix)
    {
        delete _matrix;
        _matrix = nullptr;
    }
}

bool NeoMatrixDisplay::initialize()
{
    _matrixWidth = HardwareConfiguration::DISPLAY_MATRIX_WIDTH;
    _matrixHeight = HardwareConfiguration::DISPLAY_MATRIX_HEIGHT;
    _numPixels = (uint32_t)_matrixWidth * (uint32_t)_matrixHeight;

    _leds = new CRGB[_numPixels];

    _matrix = new FastLED_NeoMatrix(
        _leds,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_W,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_H,
        HardwareConfiguration::DISPLAY_TILES_X,
        HardwareConfiguration::DISPLAY_TILES_Y,
        NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT +
            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG +
            NEO_TILE_TOP + NEO_TILE_RIGHT + NEO_TILE_COLUMNS + NEO_TILE_ZIGZAG);

    FastLED.addLeds<WS2812B, HardwareConfiguration::DISPLAY_PIN, GRB>(_leds, _numPixels);
    _matrix->setTextWrap(false);
    _matrix->setTextSize(1);
    _matrix->setBrightness(UserConfiguration::DISPLAY_BRIGHTNESS);
    clear();
    _currentFlightIndex = 0;
    _lastCycleMs = millis();
    return true;
}

void NeoMatrixDisplay::clear()
{
    if (_matrix)
    {
        _matrix->fillScreen(0);
        FastLED.show();
    }
}

String NeoMatrixDisplay::makeFlightLine(const FlightInfo &f)
{
    String airline = f.airline_display_name_full.length() ? f.airline_display_name_full
                                                          : (f.operator_iata.length() ? f.operator_iata : f.operator_icao);
    if (airline.length() == 0)
    {
        airline = f.operator_code;
    }
    String origin = f.origin.code_icao;
    String dest = f.destination.code_icao;
    String route = origin + "-" + dest;
    String type = f.aircraft_display_name_short.length() ? f.aircraft_display_name_short : f.aircraft_code;
    String ident = f.ident.length() ? f.ident : f.ident_icao;
    String line = airline;
    if (ident.length())
    {
        line += " ";
        line += ident;
    }
    if (type.length())
    {
        line += " ";
        line += type;
    }
    if (route.length() > 1)
    {
        line += " ";
        line += route;
    }
    return line;
}

void NeoMatrixDisplay::drawTextLine(int16_t x, int16_t y, const String &text, uint16_t color)
{
    _matrix->setCursor(x, y);
    _matrix->setTextColor(color);
    for (size_t i = 0; i < (size_t)text.length(); ++i)
    {
        _matrix->write(text[i]);
    }
}

String NeoMatrixDisplay::truncateToColumns(const String &text, int maxColumns)
{
    if ((int)text.length() <= maxColumns)
        return text;
    if (maxColumns <= 3)
        return text.substring(0, maxColumns);
    return text.substring(0, maxColumns - 3) + String("...");
}

bool NeoMatrixDisplay::shouldShowMapInset(const FlightInfo &f) const
{
    return UserConfiguration::DISPLAY_MAP_ENABLED &&
           f.has_live_position &&
           !isnan(f.distance_km) &&
           !isnan(f.bearing_deg) &&
           UserConfiguration::RADIUS_KM > 0.0 &&
           _matrixWidth >= 96 &&
           _matrixHeight >= 24;
}

void NeoMatrixDisplay::drawFlightMapInset(const FlightInfo &f, int16_t x, int16_t y, int16_t size, uint16_t mapColor)
{
    if (_matrix == nullptr || size < 12)
        return;

    const int16_t centerX = x + size / 2;
    const int16_t centerY = y + size / 2;
    const int16_t radius = (size / 2) - 3;
    if (radius <= 2)
        return;

    _matrix->drawRect(x, y, size, size, mapColor);
    _matrix->drawCircle(centerX, centerY, radius, mapColor);

    // Center point is the configured home/location; the dot is the selected flight.
    _matrix->drawLine(centerX - 1, centerY, centerX + 1, centerY, mapColor);
    _matrix->drawLine(centerX, centerY - 1, centerX, centerY + 1, mapColor);
    _matrix->drawPixel(centerX, y + 1, mapColor);

    double distanceRatio = f.distance_km / UserConfiguration::RADIUS_KM;
    if (distanceRatio < 0.0)
    {
        distanceRatio = 0.0;
    }
    if (distanceRatio > 1.0)
    {
        distanceRatio = 1.0;
    }

    const double bearingRad = f.bearing_deg * 3.14159265358979323846 / 180.0;
    int16_t markerX = centerX + (int16_t)round(sin(bearingRad) * radius * distanceRatio);
    int16_t markerY = centerY - (int16_t)round(cos(bearingRad) * radius * distanceRatio);
    if (markerX < x + 2)
        markerX = x + 2;
    if (markerX > x + size - 3)
        markerX = x + size - 3;
    if (markerY < y + 2)
        markerY = y + 2;
    if (markerY > y + size - 3)
        markerY = y + size - 3;

    const uint16_t markerColor = _matrix->Color(UserConfiguration::MAP_MARKER_COLOR_R,
                                                UserConfiguration::MAP_MARKER_COLOR_G,
                                                UserConfiguration::MAP_MARKER_COLOR_B);

    if (!isnan(f.heading_deg))
    {
        const double headingRad = f.heading_deg * 3.14159265358979323846 / 180.0;
        int16_t headingX = markerX + (int16_t)round(sin(headingRad) * 4.0);
        int16_t headingY = markerY - (int16_t)round(cos(headingRad) * 4.0);
        if (headingX < x + 1)
            headingX = x + 1;
        if (headingX > x + size - 2)
            headingX = x + size - 2;
        if (headingY < y + 1)
            headingY = y + 1;
        if (headingY > y + size - 2)
            headingY = y + size - 2;
        _matrix->drawLine(markerX, markerY, headingX, headingY, markerColor);
    }

    _matrix->fillCircle(markerX, markerY, 2, markerColor);
}

void NeoMatrixDisplay::displaySingleFlightCard(const FlightInfo &f)
{
    // Border
    const uint16_t borderColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                                UserConfiguration::TEXT_COLOR_G,
                                                UserConfiguration::TEXT_COLOR_B);
    _matrix->drawRect(0, 0, _matrixWidth, _matrixHeight, borderColor);

    // Calculate columns for 6x8 default font (5x7 glyphs + 1px spacing)
    const int charWidth = 6;
    const int charHeight = 8;
    const int padding = 2;                                   // Small padding from border
    const int innerWidth = _matrixWidth - 2 - (2 * padding); // Account for border and padding
    const int innerHeight = _matrixHeight - 2 - (2 * padding);
    const bool showMap = shouldShowMapInset(f);
    const int mapSize = showMap ? (innerHeight < 30 ? innerHeight : 30) : 0;
    const int16_t mapX = _matrixWidth - 1 - padding - mapSize;
    const int16_t mapY = 1 + padding + (innerHeight - mapSize) / 2;
    const int textWidth = showMap ? (mapX - (1 + padding) - padding) : innerWidth;
    const int maxCols = textWidth / charWidth;

    // Lines per display:
    // 1: airline
    // 2: route
    // 3: aircraft

    String airline = f.airline_display_name_full.length() ? f.airline_display_name_full
                                                          : (f.operator_iata.length() ? f.operator_iata : (f.operator_icao.length() ? f.operator_icao : f.operator_code));

    String origin = f.origin.code_icao;
    String dest = f.destination.code_icao;
    String line2 = origin + String(">") + dest;

    String line3 = f.aircraft_display_name_short.length() ? f.aircraft_display_name_short : f.aircraft_code;

    String line1 = truncateToColumns(airline, maxCols);
    line2 = truncateToColumns(line2, maxCols);
    line3 = truncateToColumns(line3, maxCols);

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);
    if (showMap)
    {
        drawFlightMapInset(f, mapX, mapY, mapSize, textColor);
    }

    const int lineCount = 3;
    const int lineSpacing = 1; // 1px spacing between lines
    const int totalTextHeight = lineCount * charHeight + (lineCount - 1) * lineSpacing;
    const int topOffset = 1 + padding + (innerHeight - totalTextHeight) / 2; // center inside border with padding
    const int16_t startX = 1 + padding;                                      // left padding inside border

    int16_t y = topOffset;
    drawTextLine(startX, y, line1, textColor);
    y += charHeight + lineSpacing;
    drawTextLine(startX, y, line2, textColor);
    y += charHeight + lineSpacing;
    drawTextLine(startX, y, line3, textColor);
}

void NeoMatrixDisplay::displayFlights(const std::vector<FlightInfo> &flights)
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    if (!flights.empty())
    {
        const unsigned long now = millis();
        const unsigned long intervalMs = TimingConfiguration::DISPLAY_CYCLE_SECONDS * 1000UL;

        if (flights.size() > 1)
        {
            if (now - _lastCycleMs >= intervalMs)
            {
                _lastCycleMs = now;
                _currentFlightIndex = (_currentFlightIndex + 1) % flights.size();
            }
        }
        else
        {
            _currentFlightIndex = 0;
        }

        const size_t index = _currentFlightIndex % flights.size();
        displaySingleFlightCard(flights[index]);
    }
    else
    {
        displayLoadingScreen();
    }

    FastLED.show();
}

void NeoMatrixDisplay::displayLoadingScreen()
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    const uint16_t borderColor = _matrix->Color(255, 255, 255);
    _matrix->drawRect(0, 0, _matrixWidth, _matrixHeight, borderColor);

    const int charWidth = 6;
    const int charHeight = 8;
    const String loadingText = "...";
    const int textWidth = loadingText.length() * charWidth;

    const int16_t x = (_matrixWidth - textWidth) / 2;
    const int16_t y = (_matrixHeight - charHeight) / 2 - 2;

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);
    drawTextLine(x, y, loadingText, textColor);

    FastLED.show();
}

void NeoMatrixDisplay::displayMessage(const String &message)
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    const int charWidth = 6;
    const int charHeight = 6;

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);

    // Simple single-line message; truncate if needed
    const int innerWidth = _matrixWidth;
    const int maxCols = innerWidth / charWidth;
    String line = truncateToColumns(message, maxCols);

    const int16_t x = 0;
    const int16_t y = (_matrixHeight - charHeight) / 2;
    drawTextLine(x, y, line, textColor);
    FastLED.show();
}

void NeoMatrixDisplay::showLoading()
{
    displayLoadingScreen();
}
