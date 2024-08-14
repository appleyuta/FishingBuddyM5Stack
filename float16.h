#ifndef __FLOAT16_H__
#define __FLOAT16_H__

#include <Arduino.h>

uint16_t float32ToFloat16(float value);
float float16ToFloat32(uint16_t value);

#endif
