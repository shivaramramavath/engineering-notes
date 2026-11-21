#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* UTF-8 decoder and length counter */
typedef struct {
    uint32_t codepoint;
    int bytes_read;
    bool is_valid;
} Utf8Char;

static Utf8Char utf8_decode_next(const uint8_t *s, size_t max_bytes) {
    Utf8Char result = {0, 0, false};
    if (max_bytes == 0) return result;

    uint8_t b0 = s[0];
    if (b0 <= 0x7F) {
        /* 1-byte ASCII (0xxxxxxx) */
        result.codepoint = b0;
        result.bytes_read = 1;
        result.is_valid = true;
    } else if ((b0 & 0xE0) == 0xC0) {
        /* 2-byte sequence (110xxxxx 10xxxxxx) */
        if (max_bytes < 2 || (s[1] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x1F) << 6) | (s[1] & 0x3F);
        if (cp < 0x80) return result; /* Overlong */
        result.codepoint = cp;
        result.bytes_read = 2;
        result.is_valid = true;
    } else if ((b0 & 0xF0) == 0xE0) {
        /* 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx) */
        if (max_bytes < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return result; /* Overlong or surrogate */
        result.codepoint = cp;
        result.bytes_read = 3;
        result.is_valid = true;
    } else if ((b0 & 0xF8) == 0xF0) {
        /* 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx) */
        if (max_bytes < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return result;
        uint32_t cp = (uint32_t)((b0 & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (cp < 0x10000 || cp > 0x10FFFF) return result; /* Overlong or out-of-range */
        result.codepoint = cp;
        result.bytes_read = 4;
        result.is_valid = true;
    }
    return result;
}

int main(void) {
    printf("=== Unicode and UTF-8 Decoding in C ===\n");

    /* "Hello, \u4e16\u754c!" ("Hello, 世界!") in UTF-8 hex */
    const uint8_t sample[] = "Hello, \xE4\xB8\x96\xE7\x95\x8C!";
    size_t byte_len = strlen((const char *)sample);

    printf("Input byte length: %u bytes\n", (unsigned int)byte_len);
    printf("Decoded Unicode Code Points:\n");

    const uint8_t *ptr = sample;
    size_t remaining = byte_len;
    size_t codepoint_count = 0;

    while (remaining > 0) {
        Utf8Char ch = utf8_decode_next(ptr, remaining);
        if (!ch.is_valid) {
            printf("  [Malformed UTF-8 sequence at byte offset %u]\n", (unsigned int)(ptr - sample));
            break;
        }
        printf("  U+%04X (takes %d bytes)\n", (unsigned int)ch.codepoint, ch.bytes_read);
        ptr += ch.bytes_read;
        remaining -= (size_t)ch.bytes_read;
        codepoint_count++;
    }

    printf("Total valid Unicode code points: %u\n", (unsigned int)codepoint_count);
    return 0;
}
