#include "engine.h"
#include "basic.h"
#include "game.h"
#include "renderer.h"

#include <stdarg.h>
#include <string.h>

#define GridW 10
#define GridH 20

Font BigFont;
Font DefaultFont;
Font DebugFont; // @Debug

GameControls Controls;

i32 StreakOn = false;
f32 StreakTimer = 0;
i32 Grid[GridH][GridW] = {0};
i32 PieceStatistics[array_size(Pieces)] = {0};
Scoreboard HighScore;

const char *HighScoreFileName = "highscore.dat";

const i32 BlinkTile = 200;

static b32 confirmation_prompt_open = false;
static i32 confirmation_prompt_cursor = 0;

Sprite BorderSprite;
Sprite BackgroundSprite;
Sprite PieceSprite;

Sound BackgroundSound;
Sound CursorSound;
Sound MovePieceSound;
Sound LockPieceSound;
Sound RotatePiece;
Sound GameOverSound;
Sound ScoreSound;
Sound TetrisSound;

// Using Super Rotation System

const Piece Line = {
    .side = 4,
    .type = 1,
    .bitmap = {
        0, 0, 0, 0,
        1, 1, 1, 1,
        0, 0, 0, 0,
        0, 0, 0, 0,
    },
};

const Piece Block = {
    .side = 2,
    .type = 2,
    .bitmap = {
        1, 1,
        1, 1,
    },
};

const Piece Piramid = {
    .side = 3,
    .type = 3,
    .bitmap = {
        0, 1, 0,
        1, 1, 1,
        0, 0, 0,
    },
};

const Piece LeftL = {
    .side = 3,
    .type = 4,
    .bitmap = {
        1, 0, 0,
        1, 1, 1,
        0, 0, 0,
    },
};

const Piece RightL = {
    .side = 3,
    .type = 5,
    .bitmap = {
        0, 0, 1,
        1, 1, 1,
        0, 0, 0,
    },
};

const Piece LeftZ = {
    .side = 3,
    .type = 6,
    .bitmap = {
        1, 1, 0,
        0, 1, 1,
        0, 0, 0,
    },
};

const Piece RightZ = {
    .side = 3,
    .type = 7,
    .bitmap = {
        0, 1, 1,
        1, 1, 0,
        0, 0, 0,
    },
};

const f32 BlockSize = 25.0f;

const Piece *Pieces[7] = {&Line, &Block, &Piramid, &LeftL, &RightL, &LeftZ, &RightZ};
const Vec4 PieceColors[] = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f, 1.0f},
    {0.2f, 0.3f, 0.1f, 1.0f},
};

i32 Score = 0;
b32 GameOver = false;
b32 GamePause = false;
f32 GravityCount = 0;
f32 MoveDownTime = 0.65f;

struct{
    b32 piece_setted;
    i32 x, y;
    Piece piece;
    Piece next_piece;
}Aim;

enum DebugMode{
    none,
    place_aim,
    place_piece,
    paint,
    debug_modes
};

const char *DebugModesNames[] = {"none", "place aim", "place piece", "paint", "debug_modes"};

struct{
    b32 falling;
    i32 mode;
}Debug = {.falling = true, .mode = none};

typedef struct DebugMessage_S{
    Vec4 color;
    char content[256];
}DebugMessage;

struct DebugMessageQueue_S{
    i32 start;
    i32 end;
    f32 time;
    f32 messages_time;
    DebugMessage messages[10];
}DebugMessageQueue = {.start = 0, .end = 0, .time = 0, .messages_time = 2.0f};

void update_messages(void){
    struct DebugMessageQueue_S *log = &DebugMessageQueue;

    Font *font = CurrentFont;
    set_font(&DebugFont);
    
    Vec4 bg_color = Vec4(Black_v4.x, Black_v4.y, Black_v4.z, 0.7f);
    f32 spacing = (f32)CurrentFont->line_height * 1.3f;
    f32 offset  = spacing;
    f32 x = 0.0f;
    f32 y = WHEIGHT - offset;
    f32 border = 10.0f;

    for(i32 i = log->start; i != log->end; i = (i + 1) % array_size(log->messages)){
        DebugMessage *m = &log->messages[i];
        f32 width = (f32)count_text_width(m->content) + border * 2;
        draw_rect(x, y, width, spacing, bg_color);
        draw_text(x + border, y, m->color, m->content);
        y -= spacing;
    }
    set_font(font);

    if(log->end - log->start != 0){
        log->time += TimeElapsed;
        if(log->time >= log->messages_time){
            log->start = (log->start + 1) % array_size(log->messages);
            log->time = 0;
        }
    }
}

void debug_message(Vec4 color, const char *format, ...){
    char message[member_size(DebugMessage, content)];
    format_string_varargs(message, sizeof(message), format);

    struct DebugMessageQueue_S *log = &DebugMessageQueue;
    DebugMessage *m = &log->messages[log->end];
    log->end = ++log->end % array_size(log->messages);
    if(log->start == log->end) log->start = ++log->start % array_size(log->messages);
    log->time = 0; // reset time
    strcpy(m->content, message);
    m->color = color;
}

void rotate_piece(Piece *piece, i32 dir){
    Piece original = *piece;
    for(i32 y = 0; y < piece->side; y++){
        for(i32 x = 0; x < piece->side; x++){
            if(dir < 0)
                piece->bitmap[y * piece->side + x] = original.bitmap[x * original.side + original.side - 1 - y];
            else
                piece->bitmap[y * piece->side + (piece->side - 1 - x)] = original.bitmap[x * original.side + y];
        }
    }
}

static inline Vec4 get_piece_color(i32 piece_type){
    assert(piece_type <= array_size(Pieces));
    return PieceColors[piece_type - 1];
}

b32 piece_collided(i32 x, i32 y, const Piece *piece);

Piece random_piece(void){
    return *Pieces[random_n(array_size(Pieces))];
}

void spawn_next_piece(void){
    Aim.piece = Aim.next_piece;
    Aim.next_piece = random_piece();
    Aim.piece_setted = false;
    PieceStatistics[Aim.piece.type - 1]++;

    Aim.x = (GridW - Aim.piece.side) / 2;
    Aim.y = 0;
}

b32 try_spawn_next_piece(void){
    spawn_next_piece();
    return !piece_collided(Aim.x, Aim.y, &Aim.piece);
}

void set_piece(i32 x, i32 y, const Piece *p){
    for(i32 i = 0; i < p->side; i++){
        for(i32 j = 0; j < p->side; j++){
            if(p->bitmap[i * p->side + j]){
                i32 p_x = x + j;
                i32 p_y = y + i;
                if(p_x >= GridW || p_y >= GridH || p_x < 0 || p_y < 0) continue;
                Grid[p_y][p_x] = p->type;
            }
        }
    }
}

void draw_tile(i32 x, i32 y, Vec4 color, Sprite sprite){
    draw_sprite((f32)x * BlockSize, (f32)y * BlockSize, 1.0f, color, sprite);
}

void draw_piece(i32 p_x, i32 p_y, const Piece *piece){
    for(i32 y = 0; y < piece->side; y++){
        for(i32 x = 0; x < piece->side; x++){
            if(piece->bitmap[y * piece->side + x]){
                Vec4 color = get_piece_color(piece->type);
                draw_tile(p_x + x, p_y + y, color, PieceSprite);
            }
        }
    }
}

void draw_piece_free(f32 p_x, f32 p_y, const Piece *piece){
    for(i32 y = 0; y < piece->side; y++){
        for(i32 x = 0; x < piece->side; x++){
            if(piece->bitmap[y * piece->side + x]){
                Vec4 color = get_piece_color(piece->type);
                draw_sprite(p_x + x * BlockSize, p_y + y * BlockSize, 1.0f, color, PieceSprite);
            }
        }
    }
}

b32 piece_collided(i32 x, i32 y, const Piece *p){
    for(i32 i = 0; i < p->side; i++){
        for(i32 j = 0; j < p->side; j++){
            i32 p_x = x + j;
            i32 p_y = y + i;
            if(p->bitmap[i * p->side + j] == 0) continue;

            if(p_x >= GridW || p_x < 0 || p_y >= GridH || p < 0)
                return true;
            if(Grid[p_y][p_x])
                return true;
        }
    }
    return false;
}

struct{
    i32 buffer[GridH];
    i32 count;
}GapeQueue = {.count = 0};

typedef struct{
    i32 size;
    i32 capacity;
    i32 *buffer;
}Stack;

b32 highscore_placement(i32 score, const Scoreboard *board){
    for(i32 i = 0; i < board->count; i++){
        if(score > board->score[i].score)
            return i + 1;
    }
    return board->count + 1;
}

Font load_system_font(const char *name, i32 font_size){
    char font_path[256] = {0};
    os_font_path(font_path, sizeof(font_path), "\\Fonts\\");
    b32 result = strcat_s(font_path, array_size(font_path), name) == 0;
    assert(result);
    return load_font(font_path, font_size);
}

GameControls default_game_controls(void){
    GameControls controls = { // @incomplete
        .left  = KEYCODE_A,
        .right = KEYCODE_D,
        .up    = KEYCODE_W,
        .down  = KEYCODE_S,
        .rotate_left  = KEYCODE_Q,
        .rotate_right = KEYCODE_E,
        .confirme = KEYCODE_ENTER,
        .cancel   = KEYCODE_ESC,
        .restart  = KEYCODE_O,
    };
    return controls;
}

GameControls load_controls_configuration(void){
    b32 user_config_exist = false;
    if(user_config_exist){
        assert(false); // @incomplete
    }

    return default_game_controls();
}

void EngineInit(void){
    b32 result;
    init_renderer();
    init_fonts();
    
    BigFont     = load_system_font("Arial.ttf", 36);
    DefaultFont = load_system_font("Arial.ttf", 20);
    DebugFont   = load_system_font("Consola.ttf", 16);
    set_font(&DefaultFont);

    Aim.next_piece = random_piece();
    spawn_next_piece();

    TextureInfo tile_atlas = load_texture("data\\tile_sprite.png");

    BorderSprite = (Sprite){
        .x = 0,
        .y = 0,
        .w = (i32)BlockSize,
        .h = (i32)BlockSize,
        .atlas = tile_atlas,
    };

    PieceSprite = (Sprite){
        .x = 2 * (i32)BlockSize,
        .y = 0,
        .w = (i32)BlockSize,
        .h = (i32)BlockSize,
        .atlas = tile_atlas,
    };

    BackgroundSprite = PieceSprite;

    // load saved data
    SaveData saved_data;
    result = os_read_file(&saved_data, sizeof(saved_data), SAVE_DATA_FILE_NAME);
    if(result && saved_data.save_version == SAVE_DATA_VERSION){
        HighScore = saved_data.highscore;
        Controls  = saved_data.controls;
    } else {
        debug_message(Red_v4, "Failed to read saved data! Bad version or no save found");
        set_zero(&HighScore, sizeof(HighScore));
        Controls = default_game_controls();
    }

    //BackgroundSound = load_wave_file("C:\\Windows\\Media\\Ring10.wav");
    CursorSound     = load_wave_file("data\\audio\\ui_move.wav");
    MovePieceSound  = load_wave_file("data\\audio\\move_piece.wav");
    LockPieceSound  = load_wave_file("data\\audio\\lock_piece.wav");
    RotatePiece     = load_wave_file("data\\audio\\rotate_piece.wav");
    GameOverSound   = load_wave_file("data\\audio\\gameover.wav");
    ScoreSound      = load_wave_file("data\\audio\\score.wav");
    TetrisSound     = load_wave_file("data\\audio\\tetris.wav");
    
    //play_sound(BackgroundSound, 0.2f, true);
}

static inline Vec4 invert_color(Vec4 color){
    return Vec4(1.0f - color.x, 1.0f - color.y, 1.0f - color.z, color.w);
}

const f32 first_repeat_delay   = 0.25f;
const f32 secound_repeat_delay = 0.065f;

f32 repeat_delay  = 0;
f32 pressed_timer = 0;
b32 pressed = false;

b32 key_repeat(Key k){
    if(!k.state) return false;
    if(!k.old_state) return true;
    pressed = true;
    if(pressed_timer >= repeat_delay){
        repeat_delay = secound_repeat_delay;
        return true;
    }
    return false;
}

void draw_background(i32 t_x, i32 t_y){
    const Vec4 bg_color0 = vec4_color(0xff202020);
    const Vec4 bg_color1 = vec4_color(0xff303030);

    i32 offset = 0;
    for(i32 y = t_y; y < t_y + GridH; y++){
        for(i32 x = t_x; x < t_x + GridW; x++){
            Vec4 color = x % 2 == offset? bg_color0 : bg_color1;
            draw_tile(x, y, color, BackgroundSprite);
        }
        offset = !offset;
    }
}

void draw_grid(i32 t_x, i32 t_y){
    const i32 tetris_line_start = GapeQueue.buffer[0];
    const i32 tetris_line_end   = GapeQueue.buffer[MAX(GapeQueue.count - 1, 0)];

    const i32 t_x2 = t_x - 1;
    const i32 t_y2 = t_y - 1;
    const i32 margin_w = GridW + 2;
    const i32 margin_h = GridH + 2;

    // border
    Vec4 border_color = Vec4(0.2f, 0.2f, 0.35f, 1.0f);
    for(i32 y = 0; y < margin_h; y++){
        draw_tile(t_x2, t_y2 + y, border_color, BorderSprite);
        draw_tile(t_x2 + margin_w - 1, t_y2 + y, border_color, BorderSprite);
    }

    for(i32 x = 0; x < margin_w; x++){
        draw_tile(t_x2 + x, t_y2, border_color, BorderSprite);
        draw_tile(t_x2 + x, t_y2 + margin_h - 1, border_color, BorderSprite);
    }

    // grid
    for(i32 y = 0; y < GridH; y++){
        for(i32 x = 0; x < GridW; x++){
            i32 tile = Grid[y][x];
            if(tile){
                Vec4 color = get_piece_color(tile);
                if(StreakOn && y >= tetris_line_start && y <= tetris_line_end){ // animate tetris
                    f32 ms = StreakTimer * 100.0f;
                    Vec4 blink_color = (i32)ms % 10 < 5? invert_color(color) : White_v4;
                    draw_tile(t_x + x, t_y + y, blink_color, PieceSprite);
                } else {
                    draw_tile(t_x + x, t_y + y, color, PieceSprite);
                }
            }
        }
    }
}

void draw_grid_debug(i32 t_x, i32 t_y){
    for(i32 y = 0; y < GridH; y++){
        for(i32 x = 0; x < GridW; x++){
            i32 tile = Grid[y][x];
            draw_text((x + t_x) * BlockSize, (y + t_y) * BlockSize, tile? invert_color(get_piece_color(tile)) : White_v4, "%d", tile);
        }
    }
}

b32 is_game_running(void){
    return !((GameMode != GM_Running) | GameOver | GamePause);
}

void update_grid(void){
    if(!is_game_running()) return;
    if(StreakOn){
        if(StreakTimer <= 0){
            StreakOn = false;
        } else {
            StreakTimer -= TimeElapsed;
            return;
        }
    } else {
        // Scan for complete lines to clear
        i32 streak = 0;
        for(i32 y = 0; y < GridH; y++){
            b32 is_filled = true;
            for(i32 x = 0; x < GridW; x++){
                if(Grid[y][x] == BlinkTile || Grid[y][x] == 0){
                    is_filled = false;
                    break;
                }
            }
            if(is_filled){
                streak++;
                GapeQueue.buffer[GapeQueue.count++] = y;
            }
        }

        if(streak){ // If nothing new here remove this if
            Score += streak * 1000;

            if(streak >= 4){
                StreakTimer = 1.0f;
                StreakOn = true;
                play_sound(TetrisSound, 1.0f, false);
                return;
            } else {
                play_sound(ScoreSound, 1.0f, false);
            }
        }
    }

    // Push blocks down
    i32 index = 0;
    while(index < GapeQueue.count){
        i32 goal_line = GapeQueue.buffer[index++];
        i32 *line = Grid[goal_line];
        for(i32 y = goal_line - 1; y >= 0; y--){
            memcpy(line, Grid[y], sizeof(i32) * GridW);
            line = Grid[y];
        }
    }
    GapeQueue.count = 0;
}

void save_grid(void){ // @debug
    b32 result = os_write_to_file(Grid, sizeof(Grid), DEBUG_GRID_FILE_NAME);
    if(!result){
        debug_message(Red_v4, "Can't write file!");
        return;
    }
    debug_message(Green_v4, "Grid saved!");
}

void load_grid(void){ // @debug
    b32 result = os_read_file(Grid, sizeof(Grid), DEBUG_GRID_FILE_NAME);
    if(!result){
        debug_message(Red_v4, "Can't read file!");
        return;
    }
    restart_game(false);
    debug_message(Green_v4, "Grid loaded!");
}

void restart_game(b32 clear_grid){
    if(clear_grid){
        memset(Grid, 0, sizeof(Grid)); // clean grid
    }
    Aim.next_piece = random_piece();
    spawn_next_piece();
    GravityCount = 0;
    Score = 0;
    GameOver = false;
    GamePause = false;
    memset(PieceStatistics, 0, sizeof(PieceStatistics));
}

void draw_statistics(i32 x, i32 y){
    i32 w = 5;
    i32 h = 4;

    draw_text(x * BlockSize, (y - 1) * BlockSize, White_v4, "Score:%d", Score);
    Vec4 panel_color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    draw_rect(x * BlockSize, y * BlockSize, w * BlockSize, h * BlockSize, panel_color);
    //draw_piece(x + 1, y + 1, &Aim.next_piece);
    f32 center_x = x + (w - Aim.next_piece.side) * 0.5f;
    f32 center_y = y + (h - Aim.next_piece.side) * 0.5f;
    draw_piece_free(center_x * (f32)BlockSize, center_y * (f32)BlockSize, &Aim.next_piece);

    i32 b_y = 1;
    i32 b_x = 2;

    for(i32 i = 0; i < array_size(Pieces); i++){
        const Piece *p = Pieces[i];
        i32 offset_y = i * 3;
        draw_piece(b_x, b_y + offset_y, p);
    }

    draw_centered_text((b_x + 2) * BlockSize, b_y * BlockSize * 0.6f, White_v4, "-Statistics-");
    for(i32 i = 0; i < array_size(Pieces); i++){
        i32 offset_y = i * 3;
        draw_text((b_x + 5) * BlockSize, (b_y + offset_y + 1) * BlockSize, White_v4, "%d", PieceStatistics[i]);
    }
}

void move_piece(void){
    if(!is_game_running() | StreakOn) return;

    if(key_pressed(get_key(Controls.rotate_left))){
        Piece new_pos = Aim.piece;
        rotate_piece(&new_pos, -1);
        if(!piece_collided(Aim.x, Aim.y, &new_pos)){
            Aim.piece = new_pos;
            play_sound(RotatePiece, 1.0f, false);
        }
    } else if(key_pressed(get_key(Controls.rotate_right))){
        Piece new_pos = Aim.piece;
        rotate_piece(&new_pos, 1);
        if(!piece_collided(Aim.x, Aim.y, &new_pos)){
            Aim.piece = new_pos;
            play_sound(RotatePiece, 1.0f, false);
        }
    }

    if(key_repeat(get_key(Controls.left))){
        if(!piece_collided(Aim.x - 1, Aim.y, &Aim.piece)){
            Aim.x -= 1;
            play_sound(MovePieceSound, 0.5f, false);
        }
    } else if(key_repeat(get_key(Controls.right))){
        if(!piece_collided(Aim.x + 1, Aim.y, &Aim.piece)){
            Aim.x += 1;
            play_sound(MovePieceSound, 0.5f, false);
        }
    }

    if(get_key(Controls.down).state)
        GravityCount += TimeElapsed * 10;

    if(Debug.falling)
        GravityCount += TimeElapsed;
    if(GravityCount >= MoveDownTime){
        Aim.y += 1;
        GravityCount = 0;
        if(piece_collided(Aim.x, Aim.y, &Aim.piece)){
            set_piece(Aim.x, Aim.y - 1, &Aim.piece);
            Aim.piece_setted = true;
            play_sound(LockPieceSound, 1.0f, false);
        }
    }
}

enum GameModes GameMode = GM_Menu;

void draw_scene(void){

    clear_screen(Vec4(0.1f, 0.1f, 0.1f, 0.0f));

    const i32 t_x = (WWIDTH / (i32)BlockSize - GridW) / 2;
    const i32 t_y = 1;

    draw_background(t_x, t_y);
    draw_grid(t_x, t_y);
    if(Keyboard.n0.state)
        draw_grid_debug(t_x, t_y);
    // draw aim piece
    if(!Aim.piece_setted)
        draw_piece(t_x + Aim.x, t_y + Aim.y, &Aim.piece);

    draw_statistics(t_x + GridW + 3, 3);
}

void prompt(void){
    if(button_pressed(Controls.up))
        confirmation_prompt_cursor++;
    if(button_pressed(Controls.down))
        confirmation_prompt_cursor--;
    warpi(&confirmation_prompt_cursor, 0, 1);

    if(key_pressed(get_key(Controls.confirme))){
        if(confirmation_prompt_cursor == 1){
            restart_game(true);
            debug_message(Green_v4, "Restarted!");
        }
        GameMode = GM_Running;
        confirmation_prompt_cursor = false;
    }

    draw_scene();
    Font *font_backup = CurrentFont;
    set_font(&BigFont);
    const f32 center_y = (f32)(WHEIGHT / 3);
    const f32 center_x = (f32)(WWIDTH  / 2);
    const f32 line_space = (f32)CurrentFont->line_height * 1.1f;
    i32 line = 0;
    b32 this = confirmation_prompt_cursor;
    Vec4 color = Black_v4;
    color.w = 0.3f;
    draw_rect(0, 0, WWIDTH, WHEIGHT, color);
    draw_centered_text(center_x, center_y + line_space * line++, Red_v4, "Reset game?");
    draw_centered_text(center_x, center_y + line_space * line++, this == 0? White_v4 : brightness(White_v4, 0.3f), "NO");
    draw_centered_text(center_x, center_y + line_space * line++, this == 1? White_v4 : brightness(White_v4, 0.3f), "YES");
    set_font(font_backup);
}

void game_running(void){
    // @DEBUG Save/Load grid
    if(Keyboard.r_ctrl.state && key_pressed(Keyboard.s))
        save_grid();
    if(Keyboard.r_ctrl.state && key_pressed(Keyboard.l)){
        load_grid();
    }
    if(Keyboard.r_ctrl.state && Keyboard.n.state){
        spawn_next_piece();
        PieceStatistics[Aim.piece.type - 1]++;
    }

    // @Temp Force save
    if(key_pressed(Keyboard.h)){
        save_data_to_disk();
    }

    if(key_pressed(get_key(Controls.cancel))){ // Open settings menu
        open_menu(S_Pause);
    } else if(key_pressed(get_key(Controls.restart))){ // Reset game
        confirmation_prompt_open = false;
        GameMode = GM_Prompt;
    } else if(key_pressed(get_key(Controls.confirme)) && !confirmation_prompt_open){ // Pause game
        if(GameOver){
            i32 placement = highscore_placement(Score, &HighScore);
            if(placement <= array_size(HighScore.score)){
                insert_in_scoreboard(Score, placement);
                init_highscore_menu_in_insert_mode(placement);
            } else {
                restart_game(true);
            }
        } else {
            GamePause = !GamePause;
        }
    }

    if(Aim.piece_setted && !StreakOn){
        if(!try_spawn_next_piece()){
            GameOver = true;
            play_sound(GameOverSound, 1.0f, false);
        }
    }

    move_piece();
    update_grid();
    draw_scene();

    // Debug Controls
    if(key_pressed(Keyboard.x)) Debug.falling = !Debug.falling;
    if(key_pressed(Keyboard.m)){
        Debug.mode++;
        if(Debug.mode >= debug_modes) Debug.mode = 1;
        debug_message(Red_v4, "Debug mode set [%s]", DebugModesNames[Debug.mode]);
        Debug.falling = false;
    }

    static i32 piece_index = 0; // @Debug
    if(key_pressed(Keyboard.z)){
        piece_index = (piece_index + 1) % array_size(Pieces);
    }

    const i32 t_x = (WWIDTH / (i32)BlockSize - GridW) / 2;
    const i32 t_y = 1;

    // Debug Modes
    {
        i32 m_x = Mouse.x / (i32)BlockSize - t_x;
        i32 m_y = Mouse.y / (i32)BlockSize - t_y;

        if(m_x >= 0 && m_x < GridW && m_y >= 0 && m_y < GridH){
            assert(piece_index <= array_size(Pieces));
            const Piece *piece = Pieces[piece_index];

            if(Debug.mode == place_aim){
                if(Mouse.left.state && !StreakOn){
                    Aim.piece = *piece;
                    Aim.x = m_x - 1;
                    Aim.y = m_y - 1;
                    GravityCount = 0;
                }
                draw_piece(t_x + m_x - 1, t_y + m_y - 1, piece);
            } else if(Debug.mode == place_piece){
                if(Mouse.left.state && !StreakOn)
                    set_piece(m_x - 1, m_y - 1, piece);
                draw_piece(t_x + m_x - 1, t_y + m_y - 1, piece);
            } else if(Debug.mode == paint){
                draw_rect((f32)(Mouse.x - Mouse.x % (i32)BlockSize), (f32)(Mouse.y - Mouse.y % (i32)BlockSize), BlockSize, BlockSize, Red_v4);
                if(Mouse.left.state && !StreakOn)
                    Grid[m_y][m_x] = 1;
                else if(Mouse.right.state && !StreakOn)
                    Grid[m_y][m_x] = 0;

            } else {
                assert(Debug.mode == none);
            }
        }
    }

    // Overlay
    {
        f32 center_x = (t_x + GridW / 2) * BlockSize;
        f32 center_y = (t_y + GridH / 2) * BlockSize;
        if(GameOver)
            draw_centered_text(center_x, center_y, Red_v4, "GameOver!");
        if(GamePause)
            draw_centered_text(center_x, center_y, White_v4, "-Pause-");
        if(StreakOn)
            draw_centered_text(center_x, center_y, White_v4, "Tetris!");
    }

    f32 y = WHEIGHT - (f32)CurrentFont->line_height * 1.4f;
    draw_text(10.0f, y, White_v4, "[%s-%s-%s]:move piece  [%s-%s]:rotate piece  [%s]:restart game",
        KeyNames[Controls.left],
        KeyNames[Controls.down],
        KeyNames[Controls.right],
        KeyNames[Controls.rotate_left],
        KeyNames[Controls.rotate_right],
        KeyNames[Controls.restart]
    );
}

void EngineUpdate(void){

    // dirty key system
    pressed = false;

    if(GameMode == GM_Menu){
        menu();
    } else if(GameMode == GM_Running){
        game_running();
    } else if(GameMode == GM_Prompt){
        prompt();
    } else {
        assert(false);
    }

    update_messages(); // debug

    // dirty key system
    if(!pressed){
        pressed_timer = 0;
        repeat_delay  = first_repeat_delay;
    }else{
        if(pressed_timer >= repeat_delay) pressed_timer = 0;
        pressed_timer += TimeElapsed;
    }

    // @Debug
    static f32 time_count = 0;
    time_count += TimeElapsed;
    draw_text(WWIDTH / 2, 0, Yellow_v4, "%.2f", time_count);
    show_rederer_debug_info(0, 0);
    execute_draw_commands();
    FrameDrawCallsCount = 0;
    FrameVertexCount    = 0;
}