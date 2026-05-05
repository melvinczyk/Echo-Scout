#include "display.h"

TFT_eSPI    Display::tft = TFT_eSPI();
TFT_eSprite Display::spr = TFT_eSprite(&Display::tft);

void Display::drawButton(const Button& button) {
    int bx = button.x, by = button.y;
    int bw = button.w, bh = button.h;

    tft.fillRect(bx, by, bw, bh, Colors::BG);
    tft.drawRect(bx,     by,     bw,     bh,     button.button_color);
    tft.drawRect(bx + 2, by + 2, bw - 4, bh - 4, button.button_secondary_color);
    tft.setTextColor(button.text_color, Colors::BG);
    tft.drawCentreString(button.text, bx + bw / 2, by + 9, button.font_size);
}

void Display::drawHeader(const char* title) {
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, Colors::BG);
    tft.drawFastHLine(0, HEADER_H - 1, SCREEN_W, Colors::SEP);
    tft.drawRoundRect(3, 3, 64, HEADER_H - 6, 3, Colors::GREEN_DIM);
    tft.setTextColor(Colors::GREEN_DIM, Colors::BG);
    tft.drawCentreString("< MENU", 35, 7, 2);
    tft.setTextColor(Colors::GREEN, Colors::BG);
    tft.drawCentreString(title, 120, 9, 2);
}

void Display::drawErrorScreen(const char* text) {
    tft.setTextColor(Colors::RED, Colors::BG);
    tft.drawCentreString(text, 120, 140, 2);
    tft.setTextColor(Colors::GREEN_DIM, Colors::BG);
    tft.drawCentreString("PLUG IN AND PRESS RESET", 120, 165, 1);
}

void Display::drawAsciiArt(const AsciiArt& art, Colors col) {
    tft.setTextColor(col, Colors::BG);
    for (int row = 0; row < art.num_lines; row++) {
        const char* line = art.lines[row];
        int len = strlen(line);
        for (int c = 0; c < len; c++) {
            if (line[c] == ' ') continue;
            char buf[2] = {line[c], 0};
            tft.drawString(buf, art.start_x + c * art.char_w, art.start_y + row * art.line_h, 1);
        }
    }
}

bool Display::initDisplay() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(Colors::BG);

    uint16_t probe = tft.readPixel(SCREEN_W / 2, SCREEN_H / 2);
    if (probe != Colors::BG) {
        Serial.println("[Display] initDisplay: readback mismatch — display may not be connected");
        return false;
    }

    Serial.println("[Display] initDisplay: OK");
    return true;
}
