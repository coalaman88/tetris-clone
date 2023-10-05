#define BASIC_IMPLEMENT
#define UTIL_IMPLEMENT

#pragma comment(lib, "dependencies\\freetype-lib\\x64\\freetype.lib")
#pragma comment(lib, "opengl32.lib")

#include "basic.h"
#include "engine.h"

u32 FramesPerSec;
f32 TimeElapsed;
b32 GameRunning;

struct S_Mouse Mouse;
struct S_Keyboard Keyboard = {0};

const char *KeyNames[] = {
    "none",
    "a", "b", "c", "d",
    "e", "f", "g", "h",
    "i", "j", "k", "l",
    "m", "n", "o", "p",
    "q", "r", "s", "t",
    "u", "v", "w", "x",
    "y", "z", "0", "1",
    "2", "3", "4", "5",
    "6", "7", "8", "9",
    "left", "right", "up", "down",
    "spacebar", "backspace", "shift", "enter",
    "ctrl", "plus", "minus", "asteristk",
    "r_slash", "esc",
}; static_assert(array_size(KeyNames) == KEYCODE_COUNT, "Missing key string name.");

void EngineSetup(void){
    GameRunning = true;
    EngineInit();
}

void EngineClearInput(void){
    for(i32 k = 0; k < ArraySize(Keyboard.keys); k++){
        Keyboard.keys[k].state = false;
    }

    Mouse.left.state  = false;
    Mouse.right.state = false;
}

void EngineProcessInput(void){
    for(i32 k = 0; k < ArraySize(Keyboard.keys); k++){
        Keyboard.keys[k].old_state = Keyboard.keys[k].state;
    }

    Mouse.left.old_state  = Mouse.left.state;
    Mouse.right.old_state = Mouse.right.state;
}
