#pragma once

#include "basic.h"

// New Suff

typedef struct {
    i32 side;
    i32 type;
    i32 bitmap[4 * 4];
}Piece;
extern const Piece *Pieces[7];

typedef struct{
    u32 id;
    i32 width, height;
}TextureInfo;

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
void open_settings_menu(void);

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
extern Sound MovePieceSound;
extern Sound LockPieceSound;
extern Sound RotatePiece;
extern Sound GameOverSound;
extern Sound ScoreSound;
extern Sound TetrisSound;

GameControls default_game_controls(void);