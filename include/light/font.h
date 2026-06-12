/*
----------------------------------------------------------------
Contents:

This file provides a `font` object, which consists of glyph SDF texture atlas and glyph metrics.
The font object is created from a light-font-file file, which can be generated out of other font formats, using
    utility/font_generator/light_font_file_conv.c program.

----------------------------------------------------------------
Code info:
- lfont prefix
- LIGHT_FONT_IMPL macro to build
- graphics.h dependant

----------------------------------------------------------------
Usage:
- Create font object, with a valid light-font-file linked in create info
- Use get functions to query font/glyph metrics
- lfont_get_glyph and lfont_get_kerning are O(log n) operations
- rest of get operations are O(1)

----------------------------------------------------------------
Notes:
- light-font-file are NOT tested against being malformed - user is trusted to provide proper input

----------------------------------------------------------------
Possible Optimizations:
- instead of binary searching glyphs, binary search over continuous ranges of glyphs, 
    perform O(1) array access within range - this would be faster
*/

#ifndef LIGHT_FONT_H
#define LIGHT_FONT_H

#include "light/graphics.h"
#include <stddef.h>

/*
    Dispatch for future sources
*/

// utf8 utility

static inline int lfont_utf8_decode(const char* str, size_t itr, uint32_t* codepoint);

// font type

typedef struct lfont_glyph {
    lgx_uv_2d   atlas_position;
    float       size_x;
    float       size_y;
    float       bearing_x;
    float       bearing_y;
    float       advance_x;
} lfont_glyph;

typedef struct lfont_create_info {
    size_t                  light_font_format_file_length;
    const unsigned char*    light_font_format_file_data;
} lfont_create_info;

typedef struct lfont lfont;

lfont* lfont_create_font(lgx_hardware*, lfont_create_info*);
void lfont_free_font(lfont*);

lgx_texture* lfont_get_texture(const lfont*);
lfont_glyph  lfont_get_glyph  (const lfont*, uint32_t codepoint);
float        lfont_get_kerning(const lfont*, uint32_t left_codepoint, uint32_t right_codepoint);

float lfont_get_base_size    (const lfont*);
float lfont_get_base_ascent  (const lfont*);
float lfont_get_base_descent (const lfont*);
float lfont_get_base_line_gap(const lfont*);

// inline implementations

static inline int lfont_utf8_decode(const char* str, size_t itr, uint32_t* codepoint) {
    str += itr; unsigned char c = (unsigned char)str[0];

    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    else if ((c >> 5) == 0x6) {
        *codepoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        return 2;
    }
    else if ((c >> 4) == 0xE) {
        *codepoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        return 3;
    }
    else if ((c >> 3) == 0x1E) {
        *codepoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        return 4;
    }

    // invalid fallback
    *codepoint = '?';
    return 1;
}

#endif // LIGHT_FONT_H

#ifdef LIGHT_FONT_IMPL

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct glyph_entry {
    uint32_t    codepoint;
    lfont_glyph glyph;
} glyph_entry;

typedef struct kerning_pair_entry {
    uint32_t    left_codepoint;
    uint32_t    right_codepoint;
    float       advance_x;
} kerning_pair_entry;

struct lfont {
    lgx_hardware*       owning_hardware;

    float               size;
    float               ascent;
    float               descent;
    float               line_gap;

    uint32_t            glyphs_count;
    glyph_entry*        glyphs_array;

    uint32_t            kernings_count;
    kerning_pair_entry* kernings_array;

    lgx_texture*        atlas_texture;
};

// deserialize little-endian 32-bit value
static inline uint32_t deserialize_reg_32(const unsigned char* b) {
    return (uint32_t)(
        ((uint32_t)b[0])       |
        ((uint32_t)b[1] << 8)  |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24)
    );
}

lfont* lfont_create_font(lgx_hardware* hardware, lfont_create_info* info) {
    lfont* font = calloc(1, sizeof(lfont));
    if (!font) return NULL;
    font->owning_hardware = hardware;

    const unsigned char* buf = info->light_font_format_file_data;

    #define READ_U32(target) {target = deserialize_reg_32(buf); buf += 4; } 
    #define READ_F32(target) {uint32_t as_u32; READ_U32(as_u32); memcpy(&target, &as_u32, 4);}

    // Read header
    READ_U32(font->glyphs_count);
    READ_U32(font->kernings_count);
    uint32_t texture_width;  READ_U32(texture_width);
    uint32_t texture_height; READ_U32(texture_height);
    READ_F32(font->size);
    READ_F32(font->ascent);
    READ_F32(font->descent);
    READ_F32(font->line_gap);

    if (font->glyphs_count == 0) goto _fail;

    // Load glyphs array
    font->glyphs_array = calloc(font->glyphs_count, sizeof(glyph_entry));
    if (!font->glyphs_array) goto _fail;
    for (uint32_t i = 0; i < font->glyphs_count; i++) {
        READ_U32(font->glyphs_array[i].codepoint);
        READ_F32(font->glyphs_array[i].glyph.atlas_position.min_x);
        READ_F32(font->glyphs_array[i].glyph.atlas_position.min_y);
        READ_F32(font->glyphs_array[i].glyph.atlas_position.max_x);
        READ_F32(font->glyphs_array[i].glyph.atlas_position.max_y);
        READ_F32(font->glyphs_array[i].glyph.size_x);
        READ_F32(font->glyphs_array[i].glyph.size_y);
        READ_F32(font->glyphs_array[i].glyph.bearing_x);
        READ_F32(font->glyphs_array[i].glyph.bearing_y);
        READ_F32(font->glyphs_array[i].glyph.advance_x);
    }

    // Load kernings array
    font->kernings_array = calloc(font->kernings_count, sizeof(kerning_pair_entry));
    if (!font->kernings_array) goto _fail;
    for (uint32_t i = 0; i < font->kernings_count; i++) {
        READ_U32(font->kernings_array[i].left_codepoint);
        READ_U32(font->kernings_array[i].right_codepoint);
        READ_F32(font->kernings_array[i].advance_x);
    }

    // Create texture
    lgx_texture_create_info texture_create_info = {
        .type = lgx_texture_type_2d,
        .usage = lgx_texture_usage_sampled,
        .dimensions = (lgx_texture_dimensions){
            .x = texture_width,
            .y = texture_height,
            .z = 1
        },
        .mipmap_layers   = 1,
        .array_length    = 1,
        .format          = lgx_texture_format_r8_unorm,
        .memory_access   = lgx_memory_access_allow_staging_memory_and_buffer_copy_commands_for_write,
        .memory_strategy = lgx_memory_allocation_strategy_dedicated
    };

    font->atlas_texture = lgx_create_texture(hardware, &texture_create_info);
    if (!font->atlas_texture) goto _fail;

    // Upload atlas texture
    lgx_texture_sync_upload(
        font->atlas_texture, 
        (lgx_texture_dimensions){0, 0, 0}, buf, 
        (lgx_texture_dimensions){texture_width, texture_height, 1}
    );

    return font;
_fail:
    lfont_free_font(font);
    return NULL;
}

void lfont_free_font(lfont* font) {
    if (font == NULL) return;
    free(font->glyphs_array);
    free(font->kernings_array);
    lgx_free_texture(font->atlas_texture);
    free(font);
}

lgx_texture* lfont_get_texture(const lfont* font) {
    return font->atlas_texture;
}

lfont_glyph lfont_get_glyph(const lfont* font, uint32_t codepoint) {
    int left = 0;
    int right = (int)font->glyphs_count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        uint32_t mid_codepoint = font->glyphs_array[mid].codepoint;

        if (mid_codepoint == codepoint) return font->glyphs_array[mid].glyph;
        if (mid_codepoint < codepoint)  left  = mid + 1;
        else                            right = mid - 1;
    }

    // fallback: missing glyph (return empty / zero glyph)
    return (lfont_glyph){0};
}

float lfont_get_kerning(
    const lfont* font,
    uint32_t left_codepoint,
    uint32_t right_codepoint
) {
    uint64_t key = ((uint64_t)left_codepoint << 32) | (uint64_t)right_codepoint;
    int left = 0;
    int right = (int)font->kernings_count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        uint64_t mid_key =
            ((uint64_t)font->kernings_array[mid].left_codepoint << 32) |
             (uint64_t)font->kernings_array[mid].right_codepoint;

        if (mid_key == key) return font->kernings_array[mid].advance_x;
        if (mid_key < key)  left  = mid + 1;
        else                right = mid - 1;
    }

    return 0.0f;
}

float lfont_get_base_size(const lfont* font) {
    return font->size;
}

float lfont_get_base_ascent(const lfont* font) {
    return font->ascent;
}

float lfont_get_base_descent(const lfont* font) {
    return font->descent;
}

float lfont_get_base_line_gap(const lfont* font) {
    return font->line_gap;
}

#endif // LIGHT_FONT_IMPL
