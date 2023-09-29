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

typedef struct{
    i32 x, y;
}Vec2i;
#define Vec2i(X, Y) (Vec2i){.x = (X), .y = (Y)}

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

typedef struct{
    char name[6];
    Date date;
    i64 score;
}ScoreInfo;

typedef struct{
    i32 count;
    ScoreInfo score[5];
}Scoreboard;

extern Scoreboard HighScore;

#define SCOREBOARD_FILE_VERSION "0.2"
#define SCOREBOARD_FILE_VERSION_SIZE 3

extern const char *HighScoreFileName;
b32 save_highscore_to_disk(const char *, Scoreboard *);
b32 load_highscore_from_disk(const char *, Scoreboard *);
void insert_in_scoreboard(i32 score, i32 placement);
void init_highscore_menu_in_insert_mode(i32 board_position);
void restart_game(b32 clear_grid);

void enqueue_message(Vec4 color, const char *format, ...);

extern Sound BackgroundSound;
extern Sound CursorSound;