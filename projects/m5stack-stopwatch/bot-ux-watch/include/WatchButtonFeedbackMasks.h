#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "WatchButtonFeedbackMasks.generated.h"

namespace watchbuttonmasks {

// Tokens store 1..64 pixels: literal, zero, 255, or a repeated byte.
inline bool decodeData(const uint8_t* input, size_t inputSize,
                       uint8_t* output, size_t outputSize) {
    if (!input || !output) return false;
    size_t source = 0, destination = 0;
    while (source < inputSize) {
        uint8_t token = input[source++];
        uint8_t kind = token >> 6;
        size_t length = (token & 63) + 1;
        if (destination + length > outputSize) return false;
        if (kind == 0) {
            if (source + length > inputSize) return false;
            memcpy(output + destination, input + source, length);
            source += length;
        } else if (kind == 1 || kind == 2) {
            memset(output + destination, kind == 1 ? 0 : 255, length);
        } else {
            if (source >= inputSize) return false;
            memset(output + destination, input[source++], length);
        }
        destination += length;
    }
    return destination == outputSize;
}

inline bool decode(uint8_t* output, size_t outputSize) {
    return outputSize == DecodedSize
        && decodeData(Encoded, EncodedSize, output, outputSize);
}

} // namespace watchbuttonmasks
