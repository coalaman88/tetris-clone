#ifndef ENGINE_H
#define ENGINE_H

#include "basic.h"

#define WSCALE 2
#define FWIDTH  400
#define FHEIGHT 260
#define WWIDTH  (FWIDTH * WSCALE)
#define WHEIGHT (FHEIGHT * WSCALE)
#define TARGETFPS (1000 / 60)

extern u32 FramesPerSec;
extern f32 TimeElapsed;

typedef struct{
    u8   *buffer;
    void *info;
}BitmapBuffer;

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
            Key ctrl, add, sub, mult, div, esc;
        };
    };
};
extern struct S_Keyboard Keyboard;
#define KeyPressed(key) ((key).state && !(key).old_state)

static inline b32 key_pressed_sticky(Key *k){
    return KeyPressed(*k)? k->state = false, true : false;
}

struct S_Mouse{
    i32 x, y;
    Key right, left;
};
extern struct S_Mouse Mouse;

extern b32 GameRunning;

void EngineInit();
void EngineUpdate();
void EngineDraw();
void EngineClearInput();
void EngineProcessInput();
void EngineSetup(u64 seed);

#endif
