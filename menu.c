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

b32 save_data_to_disk(void){
    SaveData data = {
        .save_version = SAVE_DATA_VERSION,
        .highscore = HighScore,
        .controls  = Controls,
    };

    return os_write_to_file(&data, sizeof(data), SAVE_DATA_FILE_NAME);
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

void open_menu(i32 destination){
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

    if(key_pressed(get_key(Controls.confirme))){
        if(main_cursor_y == 0){        // start game
            GameMode = GM_Running;
        } else if(main_cursor_y == 1){ // heighsocre
            open_menu(S_Highscore);
        } else if(main_cursor_y == 2){ // settings
            open_settings_menu();
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
    
    if(key_pressed(get_key(Controls.cancel)) && !state->insert_mode_on)
        close_menu();

    // draw
    {
        const f32 x = WWIDTH  / 2.0f;
        const f32 y = WHEIGHT / 3.0f;
        const f32 spacing_y = (f32)CurrentFont->line_height * 1.1f;
        f32 y_pen = 0;

        draw_centered_text(x, y + spacing_y * y_pen++, Blue_v4, "-Scoreboard-");

        if(key_pressed(get_key(Controls.confirme)) && state->insert_mode_on){
            if(strcmp(state->new_name.buffer, "_") != 0){
                ScoreInfo *info = &HighScore.score[state->insert_board_index];
                strcpy_s(info->name, array_size(info->name), state->new_name.buffer);
                save_data_to_disk();
                state->insert_mode_on = false;
                set_zero(&state->new_name, sizeof(state->new_name));
                close_menu();
                restart_game(true);
            }
        }

        if(state->insert_mode_on){
            if(state->new_name.count < array_size(state->new_name.buffer) - 1){
                for(i32 i = KEYCODE_A; i <= KEYCODE_Z; i++){
                    if(key_repeat(i)){
                        state->new_name.buffer[state->new_name.count++] = 'a' + (char)(i - KEYCODE_A);
                        break;
                    }
                }
            }

            if(key_repeat(KEYCODE_BACKSPACE) && state->new_name.count > 0){
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

    if(key_pressed(get_key(Controls.cancel))){
        pause_cursor_y = 0;
        close_menu();
    }

    if(key_pressed(get_key(Controls.confirme))){
        switch(pause_cursor_y){
            case 0: close_menu(); break; // resume game
            case 1: open_settings_menu(); break; // settings menu
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
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Resume"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Settings"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? White_v4 : grey, "Scoreboard"); y_pen++;
        draw_centered_text(x, y + spacing_y * y_pen, cursor_y == y_pen - 1? Red_v4 : brightness(Red_v4, 0.8f), "Exit Game"); y_pen++;
    }
}

static void draw_settings_option(f32 x, f32 y, b32 in_focus, b32 waiting_remap, i32 key_code, const char *text){
    Vec4 c = White_v4;
    Vec4 color = in_focus? (waiting_remap? Green_v4 : White_v4) : Vec4(c.x, c.y, c.z, 0.3f);
    draw_centered_text(x, y, color, "%s %s", text, KeyNames[key_code]);
}


struct S_SettingsMenuContext{
    GameControls remap;
    b32 waiting_remap;
    i32 cursor_y;
};

static struct S_SettingsMenuContext SettingsContext;

void open_settings_menu(void){
    SettingsContext.cursor_y = 0;
    SettingsContext.waiting_remap = false;
    SettingsContext.remap = Controls;
    open_menu(S_Settings);
}

static void settings_menu(void){
    struct S_SettingsMenuContext *context = &SettingsContext;

    i32 key_count  = sizeof(Controls) / sizeof(i32);
    i32 options_count = key_count + 2;

    if(context->waiting_remap){
        assert(context->cursor_y < key_count);
        i32 *buttons = (i32*)&context->remap;
        for(i32 code = KEYCODE_A; code < KEYCODE_COUNT; code++){
            if(key_pressed(Keyboard.keys[code - KEYCODE_A])){
                // clean conflicts
                for(i32 j = 0; j < key_count; j++){
                    if(buttons[j] == code) buttons[j] = KEYCODE_NONE;
                }
                buttons[context->cursor_y] = code;
                context->waiting_remap = false;
                play_sound(RotatePiece, 1.0f, false);
                break;
            }
        }
    } else {
        if(key_pressed(get_key(Controls.confirme)) && !context->waiting_remap){
            if(context->cursor_y < key_count){
                context->waiting_remap = true;
                play_sound(RotatePiece, 1.0f, false);
            } else if(context->cursor_y == key_count + 0){ // reset controls
                context->remap = default_game_controls();
                play_sound(RotatePiece, 1.0f, false);
            } else if(context->cursor_y == key_count + 1){ // save controls
                Controls = context->remap;
                play_sound(ScoreSound, 1.0f, false);
                save_data_to_disk();
                close_menu();
            }
        } else if(key_pressed(get_key(Controls.cancel))){
            close_menu();
        }

        move_cursor(&context->cursor_y, options_count - 1);
    }

    clear_screen(Vec4(0.1f, 0.1f, 0.1f, 0.0f));
    
    f32 line_height = (f32)CurrentFont->line_height;
    f32 y = WHEIGHT * .5f - line_height * options_count * 0.5f;
    f32 w = WWIDTH  * .5f;
    f32 pen_y = 0;

    draw_centered_text(w, y + line_height * (pen_y - 1), Yellow_v4, "Remap Controls");
    i32 c_y = context->cursor_y;
    b32 w_r = context->waiting_remap;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.left, "Move Left:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.right, "Move Right:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.up, "Move Up:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.down, "Move Down:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.rotate_left, "Rotate Left:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.rotate_right, "Rotate Right:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.confirme, "Confirm:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.cancel, "Cancel:"); pen_y++;
    draw_settings_option(w, y + line_height * pen_y, c_y == pen_y, w_r, context->remap.restart, "Restart:"); pen_y++;
    Vec4 color = White_v4;
    color.w = 0.3f;
    draw_centered_text(w, y + line_height * pen_y, c_y == pen_y? Red_v4 : color, "Default Controls"); pen_y++;
    draw_centered_text(w, y + line_height * pen_y, c_y == pen_y? Green_v4 : color, "Save"); pen_y++;
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