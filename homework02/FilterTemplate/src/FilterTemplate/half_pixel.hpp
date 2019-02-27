#pragma once

#include <cstdint>

void HalfpixelShift(uint8_t* field, int width, int height, bool shift_up);
void HalfpixelShift(int16_t* field, int width, int height, bool shift_up);
void HalfpixelShiftHorz(uint8_t* field, int width, int height, bool shift_up);
void HalfpixelShiftHorz(int16_t* field, int width, int height, bool shift_up);
