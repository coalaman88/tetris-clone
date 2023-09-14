#include "engine.h"
#include "game.h"
#include "draw.h"
#include "util.h"

#include <string.h> // @Remove

static i32 MenuScreen = S_Main;
static i32 original_mode;
static i32 menu_stack[10]; // max sub menu depth
static i32 menu_stack_top = 0;

static i32 pause_cursor_y = 0;
static i32 main_cursor_y = 0;

b32 load_highscore_from_disk(const char *file_name, Scoreboard *scoreboard){
    FILE *file = fopen(file_name, "rb");
    if(!file){
        return false;
    }

    size_t read = fread(scoreboard, sizeof(Scoreboard), 1, file);
    fclose(file);

    if(read < 1){
        // TODO handle this..
        assert(false);
        return false;
    }

    return true;
}

b32 save_highscore_to_disk(const char *file_name, const Scoreboard *scoreboard){
    // TODO backup
    FILE *file = fopen(file_name, "w+b");
    if(!file){
        return false;
    }

    size_t written = fwrite(scoreboard, sizeof(Scoreboard), 1, file);
    fclose(file);

    if(written < 1){
        // TODO handle this..
        assert(false);
        return false;
    }

    return true;
}

static void move_cursor(i32 *cursor, i32 max){
    i32 pos = *cursor;
    if(KeyPressed(Keyboard.s)){
        pos++;
        if(pos > max) pos = 0;
    }
    if(KeyPressed(Keyboard.w)){
        pos--;
        if(pos < 0) pos = max;
    }
    *cursor = pos;
}

void open_menu(i32 destination){ // TODO save the cursor position in the stack?
    assert(menu_stack_top + 1 < array_size(menu_stack));
    if(GameMode != GM_Menu){
        original_mode = GameMode;
        GameMode = GM_Menu;
    } else {
        menu_stack[menu_stack_top++] = MenuScreen;
    }
    MenuScreen = destination;
}

static void close_menu(void){
    if(menu_stack_top == 0){
        GameMode = original_mode;
        return;
    }
    MenuScreen = menu_stack[--menu_stack_top];
}

void main_menu(void){

    const i32 max_y = 3;
    move_cursor(&main_cursor_y, max_y);

    if(KeyPressed(Keyboard.enter)){
        if(main_cursor_y == 0){        // start game
            GameMode = GM_Running;
        } else if(main_cursor_y == 1){ // heighsocre
            open_menu(S_Highscore);
        } else if(main_cursor_y == 2){ // settings
            open_menu(S_Settings);
        } else if(main_cursor_y == 3){ // quit game
            GameRunning = false;
        } else {
            assert(false);
        }
    }

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        const f32 spacing_y = (f32)CurrentFont->line_height * 1.1f;
        const Vec4 grey = brightness(White_v4, 0.3f);
        f32 y_pen = 0;
        draw_centered_text(x, y + spacing_y * y_pen++, Yellow_v4, "Tetris");
        draw_centered_text(x, y + spacing_y * y_pen++, main_cursor_y == y_pen - 1? White_v4 : grey, "Start Game");
        draw_centered_text(x, y + spacing_y * y_pen++, main_cursor_y == y_pen - 1? White_v4 : grey, "ScoreInfo Scoreboard");
        draw_centered_text(x, y + spacing_y * y_pen++, main_cursor_y == y_pen - 1? White_v4 : grey, "Settings");
        draw_centered_text(x, y + spacing_y * y_pen++, main_cursor_y == y_pen - 1? White_v4 : grey, "Exit");
    }
}

void update_scoreboard(i32 score, i32 placement){
    if(placement > HighScore.count){
        assert(placement == HighScore.count + 1);
        HighScore.count = placement;
    }

    ScoreInfo *info = &HighScore.score[placement - 1];
    // FIXME
    info->day   = 1;
    info->month = 1;
    info->year  = 1999;
    info->score = score;
    set_zero(info->name, sizeof(info->name));
}

struct S_HighScoreMenuState{
    b32 insert_mode_on;
    i32 insert_board_index;
    struct {
        char buffer[6];
        i32 count;
    }new_name;
} HighScoreMenuState;

void init_highscore_menu_in_insert_mode(i32 board_position){
    struct S_HighScoreMenuState *state = &HighScoreMenuState;
    const char start_name[] = "_";
    state->insert_mode_on = true;
    state->insert_board_index = board_position - 1;
    strcpy_s(state->new_name.buffer, array_size(state->new_name.buffer), start_name);
    open_menu(S_Highscore);
}

void highscore_menu(void){
    // TODO
    // Option to clear highscore from game

    struct S_HighScoreMenuState *state = &HighScoreMenuState;
    
    if(KeyPressed(Keyboard.esc) && !state->insert_mode_on)
        close_menu();

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        const f32 spacing_y = (f32)CurrentFont->line_height * 1.1f;
        f32 y_pen = 0;

        draw_centered_text(x, y + spacing_y * y_pen++, Blue_v4, "-ScoreInfo Scoreboard-");

        if(KeyPressed(Keyboard.enter) && state->insert_mode_on){
            if(strcmp(state->new_name.buffer, "_") != 0){
                ScoreInfo *info = &HighScore.score[state->insert_board_index];
                strcpy_s(info->name, array_size(info->name), state->new_name.buffer);
                save_highscore_to_disk(HighScoreFileName, &HighScore);
                state->insert_mode_on = false;
                set_zero(&state->new_name, sizeof(state->new_name));
                close_menu();
                restart_game(true);
            }
        }

        if(state->insert_mode_on){
            if(state->new_name.count < array_size(state->new_name.buffer) - 1){
                for(i32 i = 0; i < 'z' - 'a'; i++){
                    if(KeyPressed(Keyboard.keys[i])){
                        state->new_name.buffer[state->new_name.count++] = 'a' + (char)i;
                        break;
                    }
                }
            }

            if(KeyPressed(Keyboard.back_space) && state->new_name.count > 0){
                i32 i = --state->new_name.count;
                state->new_name.buffer[i] = 0;
            }
        }

        if(HighScore.count > 0){
            ScoreInfo *b = HighScore.score;
            for(i32 i = 0; i < HighScore.count; i++){
                const char *name = state->insert_mode_on? state->new_name.buffer : b[0].name;
                draw_centered_text(x, y + spacing_y * y_pen++, White_v4, "%d# %s %02d/%02d/%d %d",
                    i + 1, name, b[0].month, b[0].day, b[0].year, b[0].score);
            }
        } else {
                y_pen++;
                draw_centered_text(x, y + spacing_y * y_pen, White_v4, "Empty");
        }
    }
}

void settings_menu(void){
    if(KeyPressed(Keyboard.esc))
        close_menu();

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        draw_centered_text(x, y, Red_v4, "Nothing Here!");
    }
}

void pause_menu(void){
    const i32 max_y = 2;
    move_cursor(&pause_cursor_y, max_y);

    if(KeyPressed(Keyboard.esc)){
        pause_cursor_y = 0;
        close_menu();
    }

    if(KeyPressed(Keyboard.enter)){
        switch(pause_cursor_y){
            case 0: open_menu(S_Settings); break; // settings menu
            case 1: close_menu(); break; // resume game
            case 2: GameRunning = false; break; // exit game
            default: assert(false);
        }
    }

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        const f32 spacing_y = (f32)CurrentFont->line_height * 1.1f;
        const Vec4 grey = brightness(White_v4, 0.3f);
        f32 y_pen = 0;
        draw_centered_text(x, y + spacing_y * y_pen++, Green_v4, "Pause");
        draw_centered_text(x, y + spacing_y * y_pen++, pause_cursor_y == y_pen - 1? White_v4 : grey, "Settings");
        draw_centered_text(x, y + spacing_y * y_pen++, pause_cursor_y == y_pen - 1? White_v4 : grey, "Resume");
        draw_centered_text(x, y + spacing_y * y_pen++, pause_cursor_y == y_pen - 1? Red_v4 : brightness(Red_v4, 0.8f), "Exit Game");
    }
}

void menu(void){

    clear_screen(Vec4(0.1f, 0.1f, 0.1f, 0.0f));

    set_font(&BigFont);
    switch(MenuScreen){
        case S_Main: main_menu(); break;
        case S_Pause: pause_menu(); break;
        case S_Highscore: highscore_menu(); break;
        case S_Settings: settings_menu(); break;
        default : assert(false);
    }
    set_font(&DefaultFont);
}

