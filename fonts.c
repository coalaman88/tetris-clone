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

    u32 size;
    u32 *buffer;
}GlyphAtlas;

Font *CurrentFont = NULL;

GlyphInfo append_glyph_on_atlas(GlyphAtlas *atlas, u32 unicode, FT_Face face){

    u32 glyph_index = FT_Get_Char_Index(face, unicode);
    u32 ft_error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
    assert(!ft_error);

    FT_Bitmap bitmap = face->glyph->bitmap;
    //FT_Glyph_Metrics metrics = Face->glyph->metrics;

    if(bitmap.rows >= atlas->height_left){
        printf("not enough space in font atlas!\n");
        assert(false);
    }

    if(bitmap.width >= atlas->row_width_left){
        assert(atlas->height_left > atlas->row_break);
        atlas->height_left -= atlas->row_break;
        atlas->row_break = 0;
        atlas->row_width_left = atlas->width;
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
    const u32 atlas_size = 512;
    GlyphAtlas atlas;
    atlas.width  = atlas_size;
    atlas.height = atlas_size;
    atlas.row_width_left = atlas_size;
    atlas.height_left = atlas_size;
    atlas.row_break   = 0;
    atlas.size = atlas.width * atlas.height;
    atlas.buffer = realloc(NULL, atlas.size * sizeof(u32));

    FT_Set_Pixel_Sizes(face, 0, height_pixel_size);

    Font font;
    for(i32 c = 0; c < 0xff; c++){ // loading all ascii characters
        font.glyphs[c] = append_glyph_on_atlas(&atlas, c, face);
    }

    font.atlas.id = create_texture_from_bitmap((u8*)atlas.buffer, atlas.width, atlas.height);
    font.line_height  = height_pixel_size;
    font.atlas.width  = atlas.width;
    font.atlas.height = atlas.height;

    free(atlas.buffer); // TODO use memory arena
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

i32 count_glyphs_advance(const char *string){
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

    i32 half_text_width = count_glyphs_advance(buffer) / 2;
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