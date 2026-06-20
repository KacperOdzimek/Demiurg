/*
----------------------------------------------------------------
Contents:
This file provides serialization utility

----------------------------------------------------------------
Code info:
- lse prefix
- lse_se for serialization lse_de for deserialization
- serialization in little endian
- IEEE-754 float32 format is required on target machine

----------------------------------------------------------------
Usage

Serialization:
- To serialize create zeroed lse_se_buffer
- Check with lse_se_check_buffer before writing any batch of data
- Save to buffer with lse_se_type
- Dump lse_se_buffer->buf to file

Deserialization:
- Create lse_de_buffer
- Set it's buf to file data and len to buf length
- Check with lse_de_check_buffer before reading any batch of data
- Read data
- Free memory
*/

#ifndef DEMIGURG_SERIALIZER_H
#define DEMIGURG_SERIALIZER_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <limits.h>

typedef char assert_float_size[(sizeof(float) == 4) ? 1 : -1];      // must be 32-bit float
typedef char assert_float_precision[(FLT_MANT_DIG == 24) ? 1 : -1]; // must be IEEE-754 binary32 precision
typedef char assert_float_radix[(FLT_RADIX == 2) ? 1 : -1];         // must be binary floating point

// Serialization

typedef struct lse_se_buffer {
    char*       buf;    // serialization data buffer
    uint64_t    cap;    // buffer capacity
    uint64_t    pos;    // write position
} lse_se_buffer;

// ensures buffer is capable of serializing n bytes, non zero if capable
static inline int lse_se_check_buffer(lse_se_buffer* buf, uint64_t n) {
    // find new fitting capacity
    uint64_t new_cap = buf->cap ? buf->cap : 64;
    while (new_cap < buf->pos + n) new_cap *= 2;

    // realloc if needed
    if (new_cap != buf->cap) {
        char* new_buf = (char*)realloc(buf->buf, new_cap);
        if (!new_buf) return 0;
        buf->buf = new_buf;
        buf->cap = new_cap;
    }

    return 1;
}

// pushes n bytes into serialization buffer
static inline void lse_se_bytes(lse_se_buffer* buf, uint64_t n, const void* b) {
    memcpy(buf->buf + buf->pos, b, n);
    buf->pos += n;
}

static inline void lse_se_reg_8(lse_se_buffer* buf, uint8_t v) {
    unsigned char b[] = {
        ((uint64_t)(v) >> (0 * 8)) & 0xFF,
    };
    lse_se_bytes(buf, 1, b);
}

static inline void lse_se_reg_16(lse_se_buffer* buf, uint16_t v) {
    unsigned char b[] = {
        ((uint64_t)(v) >> (0 * 8)) & 0xFF,
        ((uint64_t)(v) >> (1 * 8)) & 0xFF
    };
    lse_se_bytes(buf, 2, b);
}

static inline void lse_se_reg_32(lse_se_buffer* buf, uint32_t v) {
    unsigned char b[] = {
        ((uint64_t)(v) >> (0 * 8)) & 0xFF,
        ((uint64_t)(v) >> (1 * 8)) & 0xFF,
        ((uint64_t)(v) >> (2 * 8)) & 0xFF,
        ((uint64_t)(v) >> (3 * 8)) & 0xFF
    };
    lse_se_bytes(buf, 4, b);
}

static inline void lse_se_reg_64(lse_se_buffer* buf, uint64_t v) {
    unsigned char b[] = {
        ((uint64_t)(v) >> (0 * 8)) & 0xFF,
        ((uint64_t)(v) >> (1 * 8)) & 0xFF,
        ((uint64_t)(v) >> (2 * 8)) & 0xFF,
        ((uint64_t)(v) >> (3 * 8)) & 0xFF,
        ((uint64_t)(v) >> (4 * 8)) & 0xFF,
        ((uint64_t)(v) >> (5 * 8)) & 0xFF,
        ((uint64_t)(v) >> (6 * 8)) & 0xFF,
        ((uint64_t)(v) >> (7 * 8)) & 0xFF
    };
    lse_se_bytes(buf, 8, b);
}

static inline void lse_se_float32(lse_se_buffer* buf, float v) {
    uint32_t u; memcpy(&u, &v, sizeof(uint32_t)); // safe bit reinterpretation

    unsigned char b[] = {
        (u >> 0)  & 0xFF,
        (u >> 8)  & 0xFF,
        (u >> 16) & 0xFF,
        (u >> 24) & 0xFF
    };

    lse_se_bytes(buf, 4, b);
}

// Deserialization

typedef struct lse_de_buffer {
    const char* buf;    // lsedse_ data for deserialization
    uint64_t    len;    // lsedse_ data length bytes
    uint64_t    pos;    // read position
} lse_de_buffer;

// ensure enough data can be read, non-zero if so
static inline int lse_de_check_buffer(lse_de_buffer* buf, uint64_t n) {
    return buf->pos <= buf->len && n <= buf->len - buf->pos;
}

// reads n bytes from buffer, advances n bytes, writes into b
static inline void lse_de_bytes(lse_de_buffer* buf, uint64_t n, void* b) {
    // copy and advance
    memcpy(b, buf->buf + buf->pos, n);
    buf->pos += n;
}

static inline uint8_t lse_de_reg_8(lse_de_buffer* buf) {
    unsigned char b[1]; lse_de_bytes(buf, 1, b);
    return (uint8_t)(
        ((uint8_t)b[0])
    );
}

static inline uint16_t lse_de_reg_16(lse_de_buffer* buf) {
    unsigned char b[2]; lse_de_bytes(buf, 2, b);
    return (uint16_t)(
        ((uint16_t)b[0])       |
        ((uint16_t)b[1] << 8)
    );
}

static inline uint32_t lse_de_reg_32(lse_de_buffer* buf) {
    unsigned char b[4]; lse_de_bytes(buf, 4, b);
    return (uint32_t)(
        ((uint32_t)b[0])       |
        ((uint32_t)b[1] << 8)  |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24)
    );
}

static inline uint64_t lse_de_reg_64(lse_de_buffer* buf) {
    unsigned char b[8]; lse_de_bytes(buf, 8, b);
    return (uint64_t)(
        ((uint64_t)b[0])       |
        ((uint64_t)b[1] << 8)  |
        ((uint64_t)b[2] << 16) |
        ((uint64_t)b[3] << 24) |
        ((uint64_t)b[4] << 32) |
        ((uint64_t)b[5] << 40) |
        ((uint64_t)b[6] << 48) |
        ((uint64_t)b[7] << 56)
    );
}

static inline float lse_de_float32(lse_de_buffer* buf) {
    uint32_t u = lse_de_reg_32(buf);
    float v; memcpy(&v, &u, sizeof(uint32_t)); return v;
}

#endif // DEMIGURG_SERIALIZER_H
