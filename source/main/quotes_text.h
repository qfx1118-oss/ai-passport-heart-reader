#pragma once
#include <stdint.h>
/* Input must have passed quotes_pack_parse's strict UTF-8 validation. */
static inline uint32_t quotes_utf8_next(const char *text, uint32_t *index) {
    const unsigned char *s = (const unsigned char *)text;
    uint32_t c = s[(*index)++];
    unsigned extra = 0;
    if (c >= 0xF0) { c &= 7; extra = 3; }
    else if (c >= 0xE0) { c &= 15; extra = 2; }
    else if (c >= 0xC0) { c &= 31; extra = 1; }
    while (extra--) c = (c << 6) | (s[(*index)++] & 63);
    return c;
}
