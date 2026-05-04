#include "touch.h"


void touchInit() {
  pinMode(Config::TOUCH_RST, OUTPUT);
  digitalWrite(Config::TOUCH_RST, LOW);
  delay(10);
  digitalWrite(Config::TOUCH_RST, HIGH);
  delay(300);
  Wire.begin(Config::TOUCH_SDA, Config::TOUCH_SCL);
}

bool touchRead(int &tx, int &ty) {
  Wire.beginTransmission(Config::TOUCH_ADDR);
  Wire.write(0x02);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)Config::TOUCH_ADDR, (uint8_t)6);
  if (Wire.available() < 6)
    return false;
  uint8_t count = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  Wire.read();
  if (!count)
    return false;
  tx = ((xh & 0x0F) << 8) | xl;
  ty = ((yh & 0x0F) << 8) | yl;
  return true;
}

bool inRect(int tx, int ty, int rx, int ry, int rw, int rh) {
  return tx >= rx && tx <= rx + rw && ty >= ry && ty <= ry + rh;
}