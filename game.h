#pragma once

#include "basic.h"

// New Suff

typedef struct {
    i32 side;
    i32 type;
    i32 bitmap[4 * 4];
}Piece;
extern const Piece *Pieces[7];
#define DebugGameSeed 7024193872342

typedef struct{
    f32 position[2];
    f32 texture_coord[2];
    f32 color[4];
}Vertex;

typedef struct{
    u32 id;
    i32 width, height;
}TextureInfo;

typedef struct{
    Vertex v0, v1, v2;
    Vertex v3, v4, v5;
}Quad;

extern struct Map_S Map;
extern struct Particles_S Particles;

TextureInfo load_texture(const char *file_name);

extern char* os_read_whole_file(void *file, i32 *size);
void os_font_path(char *buffer, u32 size);

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

extern Font *CurrentFont;

// Fonts functions
void init_fonts(void);
void load_font(const char *name, i32 height_pixel_size, Font *font);
void set_font(Font *font);
void draw_font_test(void); // @Move

// Log functions
void log_printf(const char *, ...);

enum GameModes{
    GM_Running,
    GM_Menu,
    GM_Prompt,
    GM_GameModes // GameModes size
};


extern enum GameModes GameMode;

//
// Menu
//

enum MenuScreens{
    S_Main,
    S_Pause,
    S_Settings,
    S_Highscore
};

void menu(void);
void open_menu(i32 destination);

// @Move move?

typedef struct{ // @MOVE
    char name[6];
    i32 month, day, year;
    i64 score;
}ScoreInfo;

typedef struct{
    i32 count;
    ScoreInfo score[3];
}Scoreboard;

extern Scoreboard HighScore;

#define SCOREBOARD_FILE_VERSION "0.1"
#define SCOREBOARD_FILE_VERSION_SIZE 3

extern const char *HighScoreFileName;
b32 save_highscore_to_disk(const char *, Scoreboard *);
b32 load_highscore_from_disk(const char *, Scoreboard *);
void update_scoreboard(i32 score, i32 placement);
void init_highscore_menu_in_insert_mode(i32 board_position);
void restart_game(b32 clear_grid);

void enqueue_message(Vec4 color, const char *format, ...);