#ifndef TOF_H
#define TOF_H

#include <stdint.h>

// progressCb receives 0–100 as the upload progresses
bool tofInit(void (*progressCb)(uint8_t pct) = nullptr);
bool tofUpdate();

#endif
