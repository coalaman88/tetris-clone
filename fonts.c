#include "engine.h"
#include "game.h"
#include "basic.h"
#include "renderer.h"

#include <ft2build.h>
#include <freetype/freetype.h>

FT_Library Lib;

typedef struct{
    u32 width;
    u32 height;

    u32 height_left;
    u32 row_width_left;
    u32 row_break;
    u32 *buffer;
}GlyphAtlasContext;

Font *CurrentFont = NULL;

GlyphInfo append_glyph_on_atlas(GlyphAtlasContext *atlas, u32 char_index, FT_Face face){
    u32 ft_error = FT_Load_Glyph(face, char_index, FT_LOAD_RENDER);
    assert(!ft_error);

    FT_Bitmap bitmap = face->glyph->bitmap;

    if(bitmap.width >= atlas->row_width_left){
        assert(atlas->height_left > atlas->row_break);
        atlas->height_left -= atlas->row_break;
        atlas->row_break = 0;
        atlas->row_width_left = atlas->width;
    }

    if(bitmap.rows >= atlas->height_left){
        printf("not enough space in font atlas!\n");
        assert(false);
    }

    for(u32 y = 0; y < bitmap.rows; y++){
        for(u32 x = 0; x < bitmap.width; x++){
            u32 pixel = ((u32)(bitmap.buffer[x + y * bitmap.width]) << 24) | 0x00ffffff;
            u32 index = x + atlas->width - atlas->row_width_left + (atlas->height_left - y - 1) * atlas->width;
            atlas->buffer[index] = pixel;
        }
    }

    GlyphInfo g;
    g.w = bitmap.width;
    g.h = bitmap.rows;
    g.offset.x = face->glyph->bitmap_left;
    g.offset.y = face->glyph->bitmap_top;
    g.atlas.x  = atlas->width - atlas->row_width_left;
    g.atlas.y  = atlas->height - atlas->height_left;
    g.advance  = face->glyph->advance.x / 64;

    atlas->row_break = MAX(atlas->row_break, bitmap.rows);
    atlas->row_width_left -= bitmap.width;

    return g;
}

Font load_font(const char *name, i32 height_pixel_size){
    FT_Face face;
    u32 ft_error;

    ft_error = FT_New_Face(Lib, name, 0, &face);
    assert(!ft_error);

    ft_error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    assert(!ft_error);

    // Init atlas
    const u32 atlas_size  = 256;
    GlyphAtlasContext atlas = {
        .width = atlas_size,
        .height = atlas_size,
        .row_width_left = atlas_size,
        .height_left = atlas_size,
        .row_break = 0,
        .buffer = os_memory_alloc(atlas.width * atlas.height * sizeof(u32)),
    };

    Color color = rgba_color(Vec4(1, 1, 1, .2f));
    for(u32 i = 0; i < atlas.width * atlas.height; i++){
        atlas.buffer[i] = color.u;
    }

    FT_Set_Pixel_Sizes(face, 0, height_pixel_size);
    Font font;

    // load null char
    u32 char_index = FT_Get_Char_Index(face, 0);
    font.glyphs[0] = append_glyph_on_atlas(&atlas, char_index, face);

    for(i32 c = 1; c < ASCII_TABLE_SIZE; c++){ // loading all ascii characters
        char_index = FT_Get_Char_Index(face, c);
        font.glyphs[c] = char_index
            ? append_glyph_on_atlas(&atlas, char_index, face)
            : font.glyphs[0];
    }

    font.atlas.id = create_texture_from_bitmap((u8*)atlas.buffer, atlas.width, atlas.height);
    font.line_height  = height_pixel_size;
    font.atlas.width  = atlas.width;
    font.atlas.height = atlas.height;

    os_memory_free(atlas.buffer);
    FT_Done_Face(face);
    return font;
}

void set_font(Font *font){
    CurrentFont = font;
}

void init_fonts(void){
    b32 ft_error;
    ft_error = FT_Init_FreeType(&Lib);
    assert(!ft_error);
}

void render_glyph(GlyphInfo g, f32 x, f32 y, Vec4 color){
    x += g.offset.x;
    y += CurrentFont->line_height - g.offset.y;
    Sprite glyph = {.x = g.atlas.x, .y = g.atlas.y, .w = g.w, .h = g.h, .atlas = CurrentFont->atlas};
    draw_sprite(x, y, 1.0f, color, glyph);
}

i32 count_text_width(const char *string){
    i32 pen = 0;
    while(*string)
        pen += CurrentFont->glyphs[(i32)*string++].advance;
    return pen;
}

i32 draw_text(f32 x, f32 y, Vec4 color, const char *format, ...){
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);

    Vec2i pen = Vec2i(0, 0);
    const char *c = buffer;
    while(*c){
        i32 index = *c++;
        if(index == '\n'){
            pen.x = 0;
            pen.y += CurrentFont->line_height;
            continue;
        }
        GlyphInfo g = CurrentFont->glyphs[index];
        render_glyph(g, x + pen.x, y + pen.y, color);
        pen.x += g.advance;
    }
    return pen.x; // TODO return Vec2i
}

i32 draw_centered_text(f32 x, f32 y, Vec4 color, const char *format, ...){
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);

    i32 half_text_width = count_text_width(buffer) / 2;
    draw_text(x - (f32)half_text_width, y, color, buffer);
    return half_text_width;
}

i32 draw_text_warped(Rect rec, Vec4 color, const char *format, ...){
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);

    Vec2i pen = Vec2i(0, 0);
    const char *c = buffer;
    while(*c){
        i32 index = *c++;

        if(index == '\n'){
            pen.x = 0;
            pen.y += CurrentFont->line_height;
            continue;
        }

        GlyphInfo g = CurrentFont->glyphs[index];

        if(pen.x + g.advance > rec.w){
            pen.x = 0;
            pen.y += CurrentFont->line_height;
        }

        if(pen.y + CurrentFont->line_height > rec.h){
            break;
        }

        render_glyph(g, rec.x + pen.x, rec.y + pen.y, color);
        pen.x += g.advance;
    }
    return pen.x; // TODO return Vec2i
}