#pragma once

extern Vec4 White_v4, Black_v4, Red_v4, Green_v4, Blue_v4, Yellow_v4; // @MOVE this should be here??

void immediate_draw_texture(float x1, float y1, f32 scale, TextureInfo tex);
void immediate_draw_rect(f32 x1, f32 y1, f32 w, f32 h, Vec4 color);

void clear_screen(Vec4 color);
Vec4 brightness(Vec4 color, f32 scaler);

i32 draw_text(f32 x, f32 y, Vec4 color, const char *format, ...);
i32 draw_centered_text(f32 x, f32 y, Vec4 color, const char *format, ...);
i32 draw_text_warped(Rect rec, Vec4 color, const char *format, ...);
