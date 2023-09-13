#include "basic.h"
#include "engine.h"
#include <assert.h>
#include <math.h>

struct S_Keyboard Keyboard = {0};

u32 FramesPerSec;
f32 TimeElapsed;
b32 GameRunning;

struct S_Mouse Mouse;
struct S_Keyboard Keyboard;

void EngineSetup(u64 seed){
    GameRunning = 1;
    //RNGseed = (u32)(seed & 0x00000000ffffffff);
    if(seed != 0)
        RNGseed64 = seed; // From basic.h random stuff
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
