#ifndef ENGINE_H
#define ENGINE_H

#include "basic.h"

#define WSCALE 2
#define FWIDTH  400
#define FHEIGHT 300
#define WWIDTH  (FWIDTH * WSCALE)
#define WHEIGHT (FHEIGHT * WSCALE)
#define TARGETFPS (1000 / 60)

#define UI_VOLUME 0.5f

extern u32 FramesPerSec;
extern f32 TimeElapsed;

enum Enum_KeyboardKeyCodes{
    KEYCODE_NONE,
    KEYCODE_A, KEYCODE_B,
    KEYCODE_C, KEYCODE_D,
    KEYCODE_E, KEYCODE_F,
    KEYCODE_G, KEYCODE_H,
    KEYCODE_I, KEYCODE_J,
    KEYCODE_K, KEYCODE_L,
    KEYCODE_M, KEYCODE_N,
    KEYCODE_O, KEYCODE_P,
    KEYCODE_Q, KEYCODE_R,
    KEYCODE_S, KEYCODE_T,
    KEYCODE_U, KEYCODE_V,
    KEYCODE_W, KEYCODE_X,
    KEYCODE_Y, KEYCODE_Z,
    KEYCODE_0, KEYCODE_1,
    KEYCODE_2, KEYCODE_3,
    KEYCODE_4, KEYCODE_5,
    KEYCODE_6, KEYCODE_7,
    KEYCODE_8, KEYCODE_9,
    KEYCODE_LEFT, KEYCODE_RIGHT,
    KEYCODE_UP, KEYCODE_DOWN,
    KEYCODE_SPACEBAR, KEYCODE_BACKSPACE,
    KEYCODE_SHIFT, KEYCODE_ENTER,
    KEYCODE_CTRL, KEYCODE_PLUS,
    KEYCODE_MINUS, KEYCODE_ASTERISK,
    KEYCODE_RSLASH, KEYCODE_ESC,
    KEYCODE_COUNT,
};

extern const char *KeyNames[];

typedef struct{
    b32 state;
    b32 old_state;
}Key;

struct S_Keyboard{
    union{
        Key keys[50];
        struct{
            Key a, b, c, d, e, f, g, h, i, j, k, l, m;
            Key n, o, p, q, r, s, t, u, v, w, x, y, z;
            Key n0, n1, n2, n3, n4, n5, n6, n7, n8, n9;
            Key left, right, up, down;
            Key space_bar, back_space, shift, enter;
            Key r_ctrl, plus, minus, asterisk, r_slash, esc;
        };
    };
};
extern struct S_Keyboard Keyboard;

static inline b32 key_pressed(Key k){
    return k.state && !k.old_state;
}

static inline b32 key_pressed_sticky(Key *k){
    return key_pressed(*k)? k->state = false, true : false;
}

struct S_Mouse{
    i32 x, y;
    Key right, left;
};
extern struct S_Mouse Mouse;

typedef struct{
    i32 left, right, up, down;
    i32 rotate_left, rotate_right;
    i32 confirme, cancel, restart;
}GameControls;
extern GameControls Controls;

static inline Key get_key(u32 key_code){ // @Move
    assert(key_code < KEYCODE_COUNT);
    if(key_code == KEYCODE_NONE)
        return (Key){0};
    return Keyboard.keys[key_code - KEYCODE_A];
}

static inline b32 button_pressed(u32 key_code){ // @Move
    assert(key_code < KEYCODE_COUNT);
    if(key_code == 0) return false;
    return key_pressed(Keyboard.keys[key_code - KEYCODE_A]);
}

extern b32 GameRunning;

void EngineInit(void);
void EngineUpdate(void);
void EngineClearInput(void);
void EngineProcessInput(void);
void EngineSetup(void);

typedef struct {
	i16* samples;
	size_t count;
} Sound;

typedef struct{
    i16* samples;
	size_t count;
    size_t pos;
	b32 loop;
    f32 volume;
} SoundState;

Sound load_wave_file(const char *file_name);
void play_sound(Sound sound, f32 volume, b32 in_loop);
f32 sound_length(Sound sound);

typedef struct{
    i32 day;
    i32 month;
    i32 year;
}Date;

// OS functions
b32 update_file_info(void *info_handle);
void *get_file_handle(void *file_info); // @debug
void *create_file_info(void *file); // @debug
void *os_open_file(const char *name);
b32 os_close_file(void *file);
u8* os_read_whole_file(const char *name, i32 *bytes);
u8* os_read_whole_file_handle(void *file_handle, i32 *size);
b32 os_write_to_file(void *data, i32 bytes, const char *name);
b32 os_read_file(const char *name, void *buffer, i32 bytes);
char* os_font_path(char *buffer, u32 size, const char *append);
Date os_get_local_time(void);
void *os_memory_alloc(size_t bytes);
b32 os_memory_free(void *address);

#endif
