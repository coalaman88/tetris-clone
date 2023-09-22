#pragma once

typedef struct S_ShaderContext {
    u32 program_id;
    struct{
        struct FileInfo *vert_file_info, *frag_file_info;
    }debug_info;
}ShaderContext;

extern struct S_ShaderContext TextureShader;
extern struct S_ShaderContext PrimitiveShader;

extern const Vec4 White_v4, Black_v4, Red_v4, Green_v4, Blue_v4, Yellow_v4;

void init_renderer(void);
u32 create_texture_from_bitmap(u8 *data, i32 width, i32 height);
TextureInfo load_texture(const char *file_name);
void execute_draw_commands(void);

typedef struct{
    i32 x, y;
    i32 w, h;
    TextureInfo atlas;
}Sprite;

enum DrawPrimitiveTypes{
    DRAW_NONE,
    DRAW_TRIANGLE,
};

void draw_begin(i32 primitive);
void draw_end(void);
void set_color(Vec4 color);
void set_texture_coord(Vec2 coord);
void set_texture(u32 texture);
void set_shader(ShaderContext *shader);
void set_vertex(Vec2 pos);
void set_simple_quad(f32 x, f32 y, f32 w, f32 h);
void draw_sprite(float x0, float y0, f32 scale, Vec4 color, Sprite sprite);

void draw_texture(float x1, float y1, f32 scale, TextureInfo tex);
void draw_rect(f32 x1, f32 y1, f32 w, f32 h, Vec4 color);

void clear_screen(Vec4 color);
Vec4 brightness(Vec4 color, f32 scaler);

// @Debug
extern i32 FrameVertexCount;
extern i32 FrameDrawCallsCount;
void show_rederer_debug_info(f32 x, f32 y);

// Font
typedef struct{
    char character;
    i32 w, h;
    i32 advance;
    struct {i32 x, y;}offset; // FIXME Vec2i?
    struct {i32 x, y;}atlas; // FIXME Vec2i?
}GlyphInfo;

typedef struct{
    TextureInfo atlas;
    GlyphInfo glyphs[0xff]; // ascii characters
    i32 line_height; // not precise
}Font;

extern Font BigFont;
extern Font DefaultFont;
extern Font DebugFont; // @Debug
extern Font *CurrentFont;

void init_fonts(void);
Font load_font(const char *name, i32 height_pixel_size);
void set_font(Font *font);

i32 draw_text(f32 x, f32 y, Vec4 color, const char *format, ...);
i32 draw_centered_text(f32 x, f32 y, Vec4 color, const char *format, ...);
i32 draw_text_warped(Rect rec, Vec4 color, const char *format, ...);
