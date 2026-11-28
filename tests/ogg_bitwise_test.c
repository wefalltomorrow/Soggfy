#include <assert.h>
#include <stdio.h>
#include <ogg/ogg.h>

/* A byte-by-byte oracle avoids signed promotion and covers both bit orders,
   every alignment and every supported width, including the high bit. */
int main(void) {
  unsigned char bytes[8];
  unsigned int pattern, offset, width, order, i;
  for (pattern = 0; pattern < 256; ++pattern) {
    for (i = 0; i < sizeof(bytes); ++i)
      bytes[i] = (unsigned char)(pattern ^ (i * 37));
    for (order = 0; order < 2; ++order)
      for (offset = 0; offset < 8; ++offset)
        for (width = 0; width <= 32; ++width) {
          unsigned long expected = 0;
          oggpack_buffer buffer;
          for (i = 0; i < width; ++i) {
            unsigned int at = offset + i;
            unsigned int bit = (bytes[at / 8] >>
                (order ? 7 - at % 8 : at % 8)) & 1;
            expected |= (unsigned long)bit << (order ? width - 1 - i : i);
          }
          if (order) {
            oggpackB_readinit(&buffer, bytes, sizeof(bytes));
            oggpackB_adv(&buffer, offset);
            assert((unsigned long)oggpackB_look(&buffer, width) == expected);
            assert(oggpackB_bits(&buffer) == (long)offset);
            assert((unsigned long)oggpackB_read(&buffer, width) == expected);
            assert(oggpackB_bits(&buffer) == (long)(offset + width));
          } else {
            oggpack_readinit(&buffer, bytes, sizeof(bytes));
            oggpack_adv(&buffer, offset);
            assert((unsigned long)oggpack_look(&buffer, width) == expected);
            assert(oggpack_bits(&buffer) == (long)offset);
            assert((unsigned long)oggpack_read(&buffer, width) == expected);
            assert(oggpack_bits(&buffer) == (long)(offset + width));
          }
        }
  }
  puts("PASS: Ogg bit reads match independent oracle at every width/alignment/order");
  return 0;
}
