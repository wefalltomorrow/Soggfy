#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Offline 8-bit Huffman JPEG decoding. Successful decoding does not establish
 * original scan coverage, image-source provenance or replacement eligibility.
 * Use a supervised worker before exposing this to untrusted production input.
 * Every failure clears output; no borrowed output or full image is retained. */
typedef struct {
    uint32_t width,height,channels,progressive;
    size_t peak_decoder_bytes;
} SgfJpegValidation;
int SgfValidateJpeg(const uint8_t *data,size_t size,SgfJpegValidation *output);
#ifdef __cplusplus
}
#endif
