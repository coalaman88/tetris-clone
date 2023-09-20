#define BASIC_IMPLEMENT
#define UTIL_IMPLEMENT

#pragma comment(lib, "dependencies\\freetype-lib\\x64\\freetype.lib")
#pragma comment(lib, "opengl32.lib")

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

i32 StreakOn = false;
i32 StreakTimer = 0;
i32 Grid[GridH][GridW] = {0};
i32 PieceStatistics[array_size(Pieces)] = {0};
Scoreboard HighScore;

const char *HighScoreFileName = "highscore.dat";

const i32 BlinkTile = 200;

static b32 confirmation_prompt_open = false;
static i32 confirmation_prompt_cursor = 0;

// Using Super Rotation System

const Piece Line = {
    .side = 4,
    .type = 1,
    .bitmap = {
        0, 0, 0, 0,
        1, 1, 1, 1,
        0, 0, 0, 0,
        0, 0, 0, 0}
};

const Piece Block = {
    .side = 2,
    .type = 2,
    .bitmap = {
        1, 1,
        1, 1}
};

const Piece Piramid = {
    .side = 3,
    .type = 3,
    .bitmap = {
        0, 1, 0,
        1, 1, 1,
        0, 0, 0}
};

const Piece LeftL = {
    .side = 3,
    .type = 4,
    .bitmap = {
        1, 0, 0,
        1, 1, 1,
        0, 0, 0,}
};

const Piece RightL = {
    .side = 3,
    .type = 5,
    .bitmap = {
        0, 0, 1,
        1, 1, 1,
        0, 0, 0,}
};

const Piece LeftZ = {
    .side = 3,
    .type = 6,
    .bitmap = {
        1, 1, 0,
        0, 1, 1,
        0, 0, 0}
};

const Piece RightZ = {
    .side = 3,
    .type = 7,
    .bitmap = {
        0, 1, 1,
        1, 1, 0,
        0, 0, 0}
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

i32 PieceIndex = 0;
i32 Score = 0;
i32 CountTicks = 0;
b32 GameOver = false;
b32 GamePause = false;

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
    i32 time;
    i32 messages_time;
    DebugMessage messages[10];
}DebugMessageQueue = {.start = 0, .end = 0, .time = 0, .messages_time = 120};

void update_messages(void){
    struct DebugMessageQueue_S *log = &DebugMessageQueue;
    const f32 spacing = 30;
    f32 offset = (f32)CurrentFont->line_height;

    if(log->end - log->start != 0){
        log->time++; // FIXME
        if(log->time >= log->messages_time){
            log->start = ++log->start % array_size(log->messages);
            log->time = 0;
        }
    }

    for(i32 i = log->start; i != log->end; i = ++i % array_size(log->messages)){
        DebugMessage *m = &log->messages[i];
        draw_text(0, WHEIGHT - offset - 6.0f, m->color, m->content);
        offset += spacing;
    }
}

void enqueue_message(Vec4 color, const char *format, ...){
    char message[member_size(DebugMessage, content)];
    va_list args;
    va_start(args, format);
    vsprintf_s(message, sizeof(message), format, args);
    va_end(args);

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
            if(dir == -1)
                piece->bitmap[y * piece->side + x] = original.bitmap[x * original.side + original.side - 1 - y];
            else if(dir == 1)
                piece->bitmap[y * piece->side + (piece->side - 1 - x)] = original.bitmap[x * original.side + y];
            else
                assert(false);
        }
    }
}

static inline Vec4 get_piece_color(i32 piece_type){
    assert(piece_type <= array_size(Pieces));
    return PieceColors[piece_type - 1];
}

b32 piece_collided(i32 x, i32 y, const Piece *piece);

Piece random_piece(void){
    return *Pieces[random_n(array_size(Pieces) - 1)];
}

void spawn_next_piece(void){
    Aim.piece = Aim.next_piece;
    Aim.next_piece = random_piece();
    Aim.piece_setted = false;
    PieceStatistics[Aim.piece.type - 1]++;

    Aim.x = random_n(GridW - Aim.piece.side);
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

void draw_piece(i32 p_x, i32 p_y, const Piece *piece){
    for(i32 y = 0; y < piece->side; y++){
        for(i32 x = 0; x < piece->side; x++){
            if(piece->bitmap[y * piece->side + x]){
                immediate_draw_rect((p_x + x) * BlockSize, (p_y + y) * BlockSize, BlockSize, BlockSize, get_piece_color(piece->type));
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

#define Stack(B, C) {.buffer = (B), .size = 0, .capacity = (C)}

void push_stack(Stack *stack, i32 value){
    assert(stack->size < stack->capacity);
    stack->buffer[stack->size++] = value;
}

i32 pop_stack(Stack *stack){
    assert(--stack->size >= 0);
    return stack->buffer[stack->size];
}

b32 highscore_placement(i32 score, const Scoreboard *board){
    for(i32 i = 0; i < board->count; i++){
        if(score > board->score[i].score)
            return i + 1;
    }
    return board->count + 1;
}

// TODO list
// # Macro stuff
// High score db
// menu
// seting
// sound
// key remap
//
// # Micro stuff

// BUGS
// input diagonal moviment

Sprite BorderSprite;

void EngineInit(void){
    b32 result;
    init_renderer();
    init_fonts();
    
    char font_path[256] = {0};
    os_font_path(font_path, array_size(font_path));
    result = strcat_s(font_path, array_size(font_path), "\\Arial.ttf") == 0;
    assert(result);
    
    load_font(font_path, 36, &BigFont);
    load_font(font_path, 20, &DefaultFont);
    set_font(&DefaultFont);

    Aim.next_piece = random_piece();
    spawn_next_piece();

    result = load_highscore_from_disk(HighScoreFileName, &HighScore);
    if(!result){
        set_zero(&HighScore, sizeof(HighScore));
    }

    TextureInfo tile_atlas = load_texture("data\\tile_sprite.png");

    BorderSprite = (Sprite){
        .x = 0,
        .y = 0,
        .w = (i32)BlockSize,
        .h = (i32)BlockSize,
        .atlas = tile_atlas
    };
}

static inline Vec4 invert_color(Vec4 color){
    return Vec4(1.0f - color.x, 1.0f - color.y, 1.0f - color.z, color.w);
}

Vec4 hex_color(u32 hex){
    Color c = {.u = hex};
    Vec4 v  = {(f32)c.r / 255.0f, (f32)c.g / 255.0f, (f32)c.b / 255.0f, (f32)c.a / 255.0f};
    return v;
}

// FIXME make all frames to secounds!
const i32 first_repeat_delay   = 15;
const i32 secound_repeat_delay = 4;

b32 repeat_delay  = 0;
i32 pressed_timer = 0;
b32 pressed = false;

// FIXME looks bad.. also frame based
i32 DownAcc = 1;
i32 DownSpeed = 40;

b32 key_repeat(Key *k){
    if(!k->state) return false;
    if(!k->old_state) return true;
    pressed = true;
    if(pressed_timer >= repeat_delay){
        repeat_delay = secound_repeat_delay;
        return true;
    }
    return false;
}

void draw_tile(i32 x, i32 y, Vec4 color){
    immediate_draw_rect(x * BlockSize, y * BlockSize, BlockSize, BlockSize, color);
}

void draw_border_tile(i32 x, i32 y, Vec4 color){
    immediate_draw_sprite((f32)x * BlockSize, (f32)y * BlockSize, 1.0f, color, BorderSprite);
}

void draw_background(i32 t_x, i32 t_y){
    const f32 p_x = t_x * BlockSize;
    const f32 p_y = t_y * BlockSize;
    const Vec4 background_color0 = hex_color(0x222222ff);
    const Vec4 background_color1 = hex_color(0x333333ff);

    immediate_draw_rect(p_x, p_y, BlockSize * GridW, BlockSize * GridH, background_color0);
    i32 offset = 0;
    for(i32 y = t_y; y < t_y + GridH; y++){
        for(i32 x = t_x; x < t_x + GridW; x++){
            if(x % 2 == offset)
                draw_tile(x, y, background_color1);
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
    Vec4 border_color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    for(i32 y = 0; y < margin_h; y++){
        draw_border_tile(t_x2, t_y2 + y, border_color);
        draw_border_tile(t_x2 + margin_w - 1, t_y2 + y, border_color);
    }

    for(i32 x = 0; x < margin_w; x++){
        draw_border_tile(t_x2 + x, t_y2, border_color);
        draw_border_tile(t_x2 + x, t_y2 + margin_h - 1, border_color);
    }

    // grid
    for(i32 y = 0; y < GridH; y++){
        for(i32 x = 0; x < GridW; x++){
            i32 tile = Grid[y][x];
            if(tile){
                Vec4 color = get_piece_color(tile);
                if(StreakOn && y >= tetris_line_start && y <= tetris_line_end){ // animate tetris
                    draw_tile(t_x + x, t_y + y, StreakTimer % 10 < 5? invert_color(color) : White_v4);
                } else {
                    draw_tile(t_x + x, t_y + y, color);
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

void update_grid(void){
    if(GameOver || GamePause) return;
    if(StreakOn){
        if(StreakTimer <= 0){
            StreakOn = false;
        } else {
            StreakTimer--; // TODO
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
                StreakTimer = 1 * 60;
                StreakOn = true;
                return;
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
    FILE *file = fopen("grid.bin", "w+b");
    size_t written = fwrite(Grid, sizeof(Grid), 1, file);
    fclose(file);
    if(!written){
        enqueue_message(Red_v4, "Can't write file!");
        return;
    }
    enqueue_message(Green_v4, "Grid saved!");
}

void load_grid(void){ // @debug
    FILE *file = fopen("grid.bin", "rb");
    if(!file){
        enqueue_message(Red_v4, "No grid save file found!");
        return;
    }
    size_t read = fread(Grid, sizeof(Grid), 1, file);
    fclose(file);
    if(!read){
        enqueue_message(Red_v4, "Can't read file!");
        return;
    }
    enqueue_message(Green_v4, "Grid loaded!");
}

void restart_game(b32 clear_grid){
    if(clear_grid)
        memset(Grid, 0, sizeof(Grid)); // clean grid
    Aim.next_piece = random_piece();
    spawn_next_piece();
    CountTicks = 0;
    Score = 0;
    GameOver = false;
    GamePause = false;
    memset(PieceStatistics, 0, sizeof(PieceStatistics));
}

void draw_statistics(i32 x, i32 y){
    i32 w = 5;
    i32 h = 5;

    draw_text(x * BlockSize, (y - 1) * BlockSize, White_v4, "Score:%d", Score);
    Vec4 panel_color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    immediate_draw_rect(x * BlockSize, y * BlockSize, w * BlockSize, h * BlockSize, panel_color);
    draw_piece(x + 1, y + 1, &Aim.next_piece);

    i32 b_y = 1;
    i32 b_x = 2;

    draw_centered_text((b_x + 2) * BlockSize, b_y / 2.0f * BlockSize, White_v4, "-Statistics-");
    for(i32 i = 0; i < array_size(Pieces); i++){
        const Piece *p = Pieces[i];
        i32 offset_y = i * 3;
        draw_piece(b_x, b_y + offset_y, p);
        draw_text((b_x + 5) * BlockSize, (b_y + offset_y + 1) * BlockSize, White_v4, "%d", PieceStatistics[i]);
    }
}

void move_piece(void){
    if(GameOver || GamePause || StreakOn) return;

    if(KeyPressed(Keyboard.q)){
        Piece new_pos = Aim.piece;
        rotate_piece(&new_pos, -1);
        if(!piece_collided(Aim.x, Aim.y, &new_pos))
            Aim.piece = new_pos;
    } else if(KeyPressed(Keyboard.e)){
        Piece new_pos = Aim.piece;
        rotate_piece(&new_pos, 1);
        if(!piece_collided(Aim.x, Aim.y, &new_pos))
            Aim.piece = new_pos;
    }

    if(key_repeat(&Keyboard.a)){
        if(!piece_collided(Aim.x - 1, Aim.y, &Aim.piece))
            Aim.x -= 1;
    } else if(key_repeat(&Keyboard.d)){
        if(!piece_collided(Aim.x + 1, Aim.y, &Aim.piece))
            Aim.x += 1;
    }

    if(Keyboard.s.state)
        CountTicks += DownAcc * 10;

    if(Debug.falling)
        CountTicks += DownAcc;
    if(CountTicks >= DownSpeed){
        Aim.y += 1;
        CountTicks = 0;
        if(piece_collided(Aim.x, Aim.y, &Aim.piece)){
            set_piece(Aim.x, Aim.y - 1, &Aim.piece);
            Aim.piece_setted = true;
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
    if(KeyPressed(Keyboard.w))
        confirmation_prompt_cursor++;
    if(KeyPressed(Keyboard.s))
        confirmation_prompt_cursor--;
    warpi(&confirmation_prompt_cursor, 0, 1);

    if(KeyPressed(Keyboard.enter)){
        if(confirmation_prompt_cursor == 0){
            restart_game(true);
            enqueue_message(Green_v4, "Restarted!");
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
    immediate_draw_rect(0, 0, WWIDTH, WHEIGHT, color);
    draw_centered_text(center_x, center_y + line_space * line++, Red_v4, "Reset game?");
    draw_centered_text(center_x, center_y + line_space * line++, this == 0? White_v4 : brightness(White_v4, 0.3f), "YES");
    draw_centered_text(center_x, center_y + line_space * line++, this == 1? White_v4 : brightness(White_v4, 0.3f), "NO");
    set_font(font_backup);
}

void game_running(void){
    // @DEBUG Save/Load grid
    if(Keyboard.ctrl.state && KeyPressed(Keyboard.s))
        save_grid();
    if(Keyboard.ctrl.state && KeyPressed(Keyboard.l)){
        load_grid();
        restart_game(false);
    }
    if(Keyboard.ctrl.state){
        i32 i = 1;
        for(const Key *k = &Keyboard.n1; k <= &Keyboard.n9; k++){
            if(KeyPressed(*k)){
                Score = 1000 * i;
                GameOver = true;
            }
            i++;
        }
    }
    if(Keyboard.ctrl.state && Keyboard.n.state){
        spawn_next_piece();
        PieceStatistics[Aim.piece.type - 1]++;
    }

    // @Temp Force save scoreboard
    if(KeyPressed(Keyboard.h)){
        save_highscore_to_disk(HighScoreFileName, &HighScore);
    }

    // Call Settings Menu
    if(KeyPressed(Keyboard.esc)){
        open_menu(S_Pause);
        return; // FIXME skipping a drawing frame!!!
    }

    // Reset game
    if(KeyPressed(Keyboard.o)){
        confirmation_prompt_open = false;
        GameMode = GM_Prompt;
        return; // FIXME skipping a drawing frame!!!
    }

    // Pause game
    if(KeyPressed(Keyboard.enter) && !confirmation_prompt_open){
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
        GameOver = !try_spawn_next_piece();
    }

    move_piece();
    update_grid();
    draw_scene();

    // Debug Controls
    if(KeyPressed(Keyboard.x)) Debug.falling = !Debug.falling;
    if(KeyPressed(Keyboard.m)){
        Debug.mode++;
        if(Debug.mode >= debug_modes) Debug.mode = 1;
        enqueue_message(Red_v4, "Debug mode set [%s]", DebugModesNames[Debug.mode]);
        Debug.falling = false;
    }

    if(KeyPressed(Keyboard.z)){
        PieceIndex = ++PieceIndex % array_size(Pieces);
    }

    const i32 t_x = (WWIDTH / (i32)BlockSize - GridW) / 2;
    const i32 t_y = 1;

    // Debug Modes
    {
        i32 m_x = Mouse.x / (i32)BlockSize - t_x;
        i32 m_y = Mouse.y / (i32)BlockSize - t_y;

        if(m_x >= 0 && m_x < GridW && m_y >= 0 && m_y < GridH){
            assert(PieceIndex <= array_size(Pieces));
            const Piece *piece = Pieces[PieceIndex];

            if(Debug.mode == place_aim){
                if(Mouse.left.state && !StreakOn){
                    Aim.piece = *piece;
                    Aim.x = m_x - 1;
                    Aim.y = m_y - 1;
                    CountTicks = 0;
                }
                draw_piece(t_x + m_x - 1, t_y + m_y - 1, piece);
            } else if(Debug.mode == place_piece){
                if(Mouse.left.state && !StreakOn)
                    set_piece(m_x - 1, m_y - 1, piece);
                draw_piece(t_x + m_x - 1, t_y + m_y - 1, piece);
            } else if(Debug.mode == paint){
                immediate_draw_rect((f32)(Mouse.x - Mouse.x % (i32)BlockSize), (f32)(Mouse.y - Mouse.y % (i32)BlockSize), BlockSize, BlockSize, Red_v4);
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

    draw_text(10.0f, WHEIGHT - 32.0f, White_v4, "[%s%s%s%s]:move piece  [%s-%s]:rotate piece  [%s]:restart game", "w", "a", "s", "d", "q", "e", "o"); // TODO show remap keys!

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
        pressed_timer++;
    }

    //draw_text(WWIDTH - 100.0f, 0, White_v4, "FPS:%u", FramesPerSec);
    execute_draw_commands();
}

void EngineDraw(void){

}
