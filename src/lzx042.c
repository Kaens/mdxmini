/* LZX042 MDX/PDX decoder, adapted from LZX042.NAS (KUMAamp).
 * Copyright (C) Mamiya 2000. License: GPL-2.0.
 * Bounded C port by Kaens.
 */
#include "lzx042.h"
#include <stdlib.h>
#include <string.h>

#define LZX_LIMIT (16u * 1024u * 1024u)

typedef struct {
    const unsigned char *data;
    size_t size, pos;
    unsigned control, bits;
} Reader;

static int byte(Reader *r, unsigned *value) {
    if (r->pos >= r->size) return 0;
    *value = r->data[r->pos++];
    return 1;
}

static int bit(Reader *r, unsigned *value) {
    if (!r->bits) {
        if (!byte(r, &r->control)) return 0;
        r->bits = 8;
    }
    *value = (r->control >> 7) & 1;
    r->control = (r->control << 1) & 0xFF;
    --r->bits;
    return 1;
}

static int unpack(Reader r, unsigned char *out, size_t *size) {
    size_t used = 0;
    unsigned flag, value, high, low;
    for (;;) {
        size_t count, distance, i;
        if (!bit(&r, &flag)) return 0;
        if (flag) {
            if (!byte(&r, &value) || used == LZX_LIMIT) return 0;
            if (out) out[used] = (unsigned char)value;
            ++used;
            continue;
        }
        if (!bit(&r, &flag)) return 0;
        if (!flag) {
            if (!bit(&r, &high) || !bit(&r, &low) || !byte(&r, &value)) return 0;
            count = 2 + (high << 1) + low;
            distance = 0x100 - value;
        } else {
            if (!byte(&r, &high) || !byte(&r, &low)) return 0;
            distance = 8192 - ((high * 0x100 + low) >> 3);
            count = low & 7;
            if (count) count += 2;
            else {
                if (!byte(&r, &value)) return 0;
                if (!value) { *size = used; return 1; }
                count = value + 1;
            }
        }
        if (distance > used || count > LZX_LIMIT - used) return 0;
        for (i = 0; out && i < count; ++i) out[used + i] = out[used + i - distance];
        used += count;
    }
}

int mdx_lzx042(const unsigned char *data, size_t size,
               unsigned char **out, size_t *out_size) {
    Reader r;
    size_t marker, decoded;
    unsigned char *buffer;
    *out = NULL;
    *out_size = 0;
    if (!data || size < 12 || memcmp(data + 4, "LZX 0.42", 8)) return 0;
    /* the original decoder starts at 0x24, then scans on even addresses */
    for (marker = 0x26; marker + 4 <= size; marker += 2) {
        if (!memcmp(data + marker, "\x7f\xff\xff\x4c", 4)) break;
    }
    if (marker + 4 >= size) return -1;

    r.data = data;
    r.size = size;
    r.pos = marker + 4;
    r.control = r.bits = 0;
    if (!unpack(r, NULL, &decoded) || !decoded) return -1; /* validate before committing to calloc */
    buffer = (unsigned char *)calloc(decoded + 16, 1);
    if (!buffer) return -1;
    if (!unpack(r, buffer, &decoded)) { free(buffer); return -1; }
    *out = buffer;
    *out_size = decoded;
    return 1;
}
