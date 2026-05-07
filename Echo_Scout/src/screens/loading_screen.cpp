#include "screens/loading_screen.h"
#include "base/display.h"
#include "screens/ascii_art.h"


void drawLoadingScreen() {
    Display::AsciiArt scout = {AsciiArtInstance::SCOUT, std::size(AsciiArtInstance::SCOUT), 4, 8, 64, 100};
    Display::AsciiArt echo = {AsciiArtInstance::ECHO, std::size(AsciiArtInstance::ECHO), 4, 8, 34, 50};
    
    Display::tft.fillScreen(Display::Colors::BG);

    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::drawAsciiArt(echo, Display::Colors::GREEN);

    Display::tft.setTextColor(Display::Colors::GREEN, Display::Colors::BG);
    Display::drawAsciiArt(scout, Display::Colors::GREEN);

    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString("FLASHING SENSOR FIRMWARE", 120, 162, 1);

    Display::tft.drawRect(LoadingScreen::LD_BAR_X - 1, LoadingScreen::LD_BAR_Y - 1, LoadingScreen::LD_BAR_W + 2, LoadingScreen::LD_BAR_H + 2, Display::Colors::GREEN_DIM);
}


void updateLoadingBar(uint8_t pct) {
    if (pct == LoadingScreen::ld_prevPct) return;
    LoadingScreen::ld_prevPct = pct;

    int filled = static_cast<int>(LoadingScreen::LD_BAR_W * pct / 100);
    Display::tft.fillRect(LoadingScreen::LD_BAR_X, LoadingScreen::LD_BAR_Y, filled, LoadingScreen::LD_BAR_H, Display::Colors::GREEN);
    if (filled < LoadingScreen::LD_BAR_W)
        Display::tft.fillRect(LoadingScreen::LD_BAR_X + filled, LoadingScreen::LD_BAR_Y,
                              LoadingScreen::LD_BAR_W - filled, LoadingScreen::LD_BAR_H, Display::Colors::BG);
    char buf[8]; sprintf(buf, "%d%%", pct);
    Display::tft.setTextColor(Display::Colors::GREEN_DIM, Display::Colors::BG);
    Display::tft.drawCentreString(buf, 120, LoadingScreen::LD_BAR_Y + LoadingScreen::LD_BAR_H + 6, 1);
}