#include "engine.h"
#include "util.h"
#include "game.h"
#include "basic.h"
#include "render.h"
#include "draw.h"

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

void load_font(const char *name, i32 height_pixel_size, Font *font){
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

    for(i32 c = 0; c < 0xff; c++){ // loading all ascii characters
        font->glyphs[c] = append_glyph_on_atlas(&atlas, c, face);
    }


    // creating opengl texture
    font->line_height = height_pixel_size;
    font->atlas.width = atlas.width;
    font->atlas.height = atlas.height; // TODO make the height be only the used?

    glGenTextures(1, &font->atlas.id);
    glBindTexture(GL_TEXTURE_2D, font->atlas.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlas.width, atlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    free(atlas.buffer); // TODO use memory arena
    FT_Done_Face(face);
}

void set_font(Font *font){
    CurrentFont = font;
}

void init_fonts(void){
    b32 ft_error;
    ft_error = FT_Init_FreeType(&Lib);
    assert(!ft_error);
}

void render_glyph(GlyphInfo g, f32 x1, f32 y1, Vec4 color){
    x1 += g.offset.x;
    y1 += CurrentFont->line_height - g.offset.y;

    f32 tex_x1 = (f32)g.atlas.x / CurrentFont->atlas.width;
    f32 tex_x2 = (f32)(g.atlas.x + g.w) / CurrentFont->atlas.width;
    f32 tex_y1 = 1.0f - (f32)(g.atlas.y + g.h) / CurrentFont->atlas.height;
    f32 tex_y2 = 1.0f - (f32)g.atlas.y / CurrentFont->atlas.height;

    f32 y2 = y1 + g.h;
    f32 x2 = x1 + g.w;

    Quad new_quad = {
        {{ x1, y1 }, { tex_x1, tex_y2 }, { color.x, color.y, color.z, color.w }},
        {{ x1, y2 }, { tex_x1, tex_y1 }, { color.x, color.y, color.z, color.w }},
        {{ x2, y2 }, { tex_x2, tex_y1 }, { color.x, color.y, color.z, color.w }},
        {{ x2, y2 }, { tex_x2, tex_y1 }, { color.x, color.y, color.z, color.w }},
        {{ x2, y1 }, { tex_x2, tex_y2 }, { color.x, color.y, color.z, color.w }},
        {{ x1, y1 }, { tex_x1, tex_y2 }, { color.x, color.y, color.z, color.w }},
    };

    push_render_quad_command(&TextureShader, CurrentFont->atlas.id, &new_quad);
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

void draw_font_test(void){
    i32 c = 'f';
    render_glyph(CurrentFont->glyphs[c], 200, 200, White_v4);
    immediate_draw_texture(100, 0, 1.0f, CurrentFont->atlas);
    draw_text(300, 300, White_v4, "Renderizando glifos corretamente finalmente?");
    draw_text(300, 400, White_v4, "Float:%f String:%s Integer:%d", PI, "lion", 42);
}
