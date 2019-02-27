#pragma once

#include <cstdint>

/// Compute SAD between two 16x16 blocks
long GetErrorSAD_16x16(const uint8_t* block1, const uint8_t* block2, int stride);

/// Compute SAD between two 8x8 blocks
long GetErrorSAD_8x8(const uint8_t* block1, const uint8_t* block2, int stride);
