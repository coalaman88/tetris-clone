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

// Key states
static const f32 first_repeat_delay   = 0.25f;
static const f32 secound_repeat_delay = 0.065f;
static f32 repeat_delay  = 0.25f;
static f32 pressed_timer = 0;
static i32 key_to_repeat = KEYCODE_NONE;

void engine_setup(void){
    GameRunning = true;
    engine_init();
}

void engine_clear_input(void){
    for(i32 k = 0; k < ArraySize(Keyboard.keys); k++){
        Keyboard.keys[k].state = false;
    }

    Mouse.left.state  = false;
    Mouse.right.state = false;
}

void engine_process_input(void){
    // update key state
    for(i32 k = 0; k < ArraySize(Keyboard.keys); k++){
        Keyboard.keys[k].old_state = Keyboard.keys[k].state;
    }

    Mouse.left.old_state  = Mouse.left.state;
    Mouse.right.old_state = Mouse.right.state;

    // key repetition text input style
    if(!get_key(key_to_repeat).state){
        key_to_repeat = KEYCODE_NONE;
    } else {
        if(pressed_timer >= repeat_delay){
            pressed_timer = 0;
            repeat_delay  = secound_repeat_delay;
        }
        pressed_timer += TimeElapsed;    
    }
}

b32 key_repeat(i32 keycode){
    if(key_pressed(get_key(keycode))){
        key_to_repeat = keycode;
        pressed_timer = 0;
        repeat_delay  = first_repeat_delay;
        return true;
    }

    return (key_to_repeat == keycode) & (pressed_timer >= repeat_delay);
}