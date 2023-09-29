#include "engine.h"
#include "game.h"
#include "renderer.h"

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
    
    char version[SCOREBOARD_FILE_VERSION_SIZE + 1];
    size_t read = fread(version, SCOREBOARD_FILE_VERSION_SIZE, 1, file);
    version[sizeof(version) - 1] = 0;

    if(read < 1){
        enqueue_message(Red_v4, "unable to read highscore file!");
        fclose(file);
        return false;
    }

    if(strcmp(version, SCOREBOARD_FILE_VERSION) != 0){
        enqueue_message(Red_v4, "highscore data with different version! %s", version);
        fclose(file);
        return false;
    }

    Scoreboard temp_scoreboard = {0};
    read = fread(&temp_scoreboard, sizeof(Scoreboard), 1, file);
    fclose(file);
    if(read < 1){
        enqueue_message(Red_v4, "unable to read highscore file!");
        return false;
    }

    *scoreboard = temp_scoreboard;
    return true;
}

b32 save_highscore_to_disk(const char *file_name, Scoreboard *scoreboard){
    // TODO backup
    FILE *file = fopen(file_name, "w+b");
    if(!file){
        return false;
    }
    size_t written;
    
    written = fwrite(SCOREBOARD_FILE_VERSION, SCOREBOARD_FILE_VERSION_SIZE, 1, file);
    if(written < 1){
        // TODO handle this..
        assert(false);
        return false;
    }

    written = fwrite(scoreboard, sizeof(Scoreboard), 1, file);
    fclose(file);

    if(written < 1){
        // TODO handle this..
        assert(false);
        return false;
    }

    return true;
}

static void move_cursor(i32 *cursor, i32 max){
    if(button_pressed(Controls.down))
        *cursor += 1;
    else if(button_pressed(Controls.up))
        *cursor -= 1;
    else return;
    warpi(cursor, 0, max);
    play_sound(CursorSound, UI_VOLUME, false);
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

static void main_menu(void){

    const i32 max_y = 3;
    move_cursor(&main_cursor_y, max_y);

    if(key_pressed(Keyboard.enter)){
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
        i32 cursor_y = main_cursor_y;
        draw_centered_text(x, y + spacing_y * y_pen, Yellow_v4, "Tetris"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Start Game"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Scoreboard"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Settings"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Exit"); y_pen++;
    }
}

void insert_in_scoreboard(i32 score, i32 placement){
    assert(placement <= array_size(HighScore.score));
    if(HighScore.count < array_size(HighScore.score))
        HighScore.count++;
    assert(placement <= HighScore.count);

    i32 insert_pos = placement - 1;
    i32 last_slot = HighScore.count - 1;
    for(i32 i = last_slot - 1; i >= insert_pos; i--){
        HighScore.score[i + 1] = HighScore.score[i];
    }

    ScoreInfo *info = &HighScore.score[insert_pos];
    info->date  = os_get_local_time();
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

static void highscore_menu(void){
    // TODO
    // Option to clear highscore from game

    struct S_HighScoreMenuState *state = &HighScoreMenuState;
    
    if(key_pressed(Keyboard.esc) && !state->insert_mode_on)
        close_menu();

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        const f32 spacing_y = (f32)CurrentFont->line_height * 1.1f;
        f32 y_pen = 0;

        draw_centered_text(x, y + spacing_y * y_pen++, Blue_v4, "-Scoreboard-");

        if(key_pressed(Keyboard.enter) && state->insert_mode_on){
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
                    if(key_pressed(Keyboard.keys[i])){
                        state->new_name.buffer[state->new_name.count++] = 'a' + (char)i;
                        break;
                    }
                }
            }

            if(key_pressed(Keyboard.back_space) && state->new_name.count > 0){
                i32 i = --state->new_name.count;
                state->new_name.buffer[i] = 0;
            }
        }

        if(HighScore.count > 0){
            ScoreInfo *b = HighScore.score;
            for(i32 i = 0; i < HighScore.count; i++){
                b32 is_inserting = state->insert_mode_on && i == state->insert_board_index;
                const char *name = is_inserting? state->new_name.buffer : b[i].name;
                Date date = b[i].date;
                draw_centered_text(x, y + spacing_y * y_pen++, White_v4, "%d# %s %02d/%02d/%d Score: %d",
                    i + 1, name, date.month, date.day, date.year, b[i].score);
            }
        } else {
                y_pen++;
                draw_centered_text(x, y + spacing_y * y_pen, White_v4, "Empty");
        }
    }
}

static void pause_menu(void){
    const i32 max_y = 3;
    move_cursor(&pause_cursor_y, max_y);

    if(key_pressed(Keyboard.esc)){
        pause_cursor_y = 0;
        close_menu();
    }

    if(key_pressed(Keyboard.enter)){
        switch(pause_cursor_y){
            case 0: open_menu(S_Settings); break; // settings menu
            case 1: close_menu(); break; // resume game
            case 2: open_menu(S_Highscore); break; // scoreboard
            case 3: GameRunning = false; break; // exit game
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
        i32 cursor_y = pause_cursor_y;
        draw_centered_text(x, y + spacing_y * y_pen, Green_v4, "Pause"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Settings"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Resume"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Scoreboard"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? Red_v4 : brightness(Red_v4, 0.8f), "Exit Game"); y_pen++;
    }
}

static void draw_settings_option(f32 x, f32 y, b32 in_focus, b32 waiting_remap, i32 key_code, const char *text){
    Vec4 color = in_focus? (waiting_remap? Green_v4 : White_v4) : Vec4(1.0f, 1.0f, 1.0f, 0.3f);
    draw_centered_text(x, y, color, "%s %s", text, KeyNames[key_code]);
}

static void settings_menu(void){
    static i32 waiting_remap = false;
    static i32 cursor_y = 0;
    i32 opetions_count = 4;

    if(key_pressed(Keyboard.esc)){
        waiting_remap = false;
        close_menu();
    }

    if(waiting_remap){
        i32 *buttons[] = {
            &Controls.left,
            &Controls.right,
            &Controls.up,
            &Controls.down,
        };
        assert(array_size(buttons) == opetions_count);
        // TODO solve conflicting keys!
        i32 *selected_button = buttons[cursor_y];
    	for(i32 i = KEYCODE_A; i < KEYCODE_COUNT; i++){
            if(i == KEYCODE_ESC) continue;
            if(key_pressed(Keyboard.keys[i])){
                *selected_button = i;
                waiting_remap = false;
                break;
            }
        }
    } else {
        move_cursor(&cursor_y, opetions_count - 1);
        if(key_pressed(Keyboard.enter) && !waiting_remap){
            waiting_remap = true;
        }
    }

    f32 line_height = (f32)CurrentFont->line_height;
    f32 y = WHEIGHT * .5f - line_height * opetions_count * 0.5f;
    f32 w = WWIDTH  * .5f;
    f32 pen_y = 0;

    // TODO incomplete
    clear_screen(Vec4(0.1f, 0.1f, 0.1f, 0.0f));
    draw_settings_option(w, y + line_height * pen_y, cursor_y == pen_y, waiting_remap, Controls.left, "Move Left:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, cursor_y == pen_y, waiting_remap, Controls.right, "Move Right:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, cursor_y == pen_y, waiting_remap, Controls.up, "Move Up:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, cursor_y == pen_y, waiting_remap, Controls.down, "Move Down:"); pen_y++;
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