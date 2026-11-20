#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Complete static PNG decoding, not artwork provenance or permission to replace.
 * Not connected to production. Call only in a supervised worker before use on
 * untrusted files: memory limits do not supply a CPU deadline or cancellation.
 * No borrowed output; failure clears the entire result. */
typedef struct {
    uint32_t width, height, bit_depth, channels;
    size_t peak_decoder_bytes;
} SgfPngValidation;
int SgfValidatePng(const uint8_t *bytes, size_t size, SgfPngValidation *output);
#ifdef __cplusplus
}
#endif
