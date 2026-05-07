#ifndef TOF_H
#define TOF_H

#include <stdint.h>

bool tofInit(void (*progressCb)(uint8_t pct) = nullptr);
bool tofUpdate();

#endif
