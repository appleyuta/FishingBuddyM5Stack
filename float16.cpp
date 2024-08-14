#include "float16.h"

// float32をfloat16に変換する関数
uint16_t float32ToFloat16(float value) {
    uint32_t bits = *((uint32_t*)&value);
    uint16_t sign = (bits >> 16) & 0x8000;
    uint16_t exponent = ((bits >> 23) & 0xFF) - 112;
    uint16_t fraction = (bits >> 13) & 0x3FF;

    if (exponent <= 0) {
        // Subnormal number or zero
        exponent = 0;
        fraction = 0;
    } else if (exponent >= 31) {
        // Overflow to infinity or NaN
        exponent = 31;
        fraction = 0;
    }

    return sign | (exponent << 10) | fraction;
}

// float16をfloat32に変換する関数
float float16ToFloat32(uint16_t value) {
    uint32_t sign = (value & 0x8000) << 16;
    uint32_t exponent = (value & 0x7C00) >> 10;
    uint32_t fraction = (value & 0x03FF) << 13;

    if (exponent == 0) {
        // Subnormal number or zero
        exponent = 0;
    } else if (exponent == 31) {
        // Infinity or NaN
        exponent = 255;
    } else {
        exponent += 112;
    }

    uint32_t bits = sign | (exponent << 23) | fraction;
    return *((float*)&bits);
}