#ifndef BASIC_H
#define BASIC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

typedef int32_t   i32;
typedef uint32_t  u32;
typedef int64_t   i64;
typedef uint64_t  u64;
typedef int16_t   i16;
typedef uint16_t  u16;
typedef int8_t    i8;
typedef uint8_t   u8;
typedef float     f32;
typedef double    f64;
typedef i8        bool;
typedef i32       b32;
typedef u64       memptr;

#define true  1
#define false 0

#define ArraySize(X) (i64)(sizeof(X) / sizeof(X[0]))
#define array_size(X) (i64)(sizeof(X) / sizeof(X[0]))
#define member_size(type, member) sizeof(((type *)0)->member)

#define Kilobyte(X) ((X) * 1024)
#define Megabyte(X) ((X) * 1024 * 1024)
#define Gigabyte(X) ((X) * 1073741824llu)

#define MAXi32 0x7fffffff
#define MAXi64 0x7fffffffffffffff
#define MAXu32 0xffffffff
#define MAXu64 0xffffffffffffffff
#define MINi32 0x80000000
#define MINi64 0x8000000000000000

// ===================================================================
// Math
// ===================================================================

#define PI 3.14159265358979323846
#define Radians(degres) (degres)*PI/180.0f

#define MIN(A, B) ((A) < (B)? (A) : (B))
#define MAX(A, B) ((A) > (B)? (A) : (B))

typedef struct{
    f32 x, y;
}Vec2;

#define Vec2(X, Y) (Vec2){.x = (X), .y = (Y)}
#define AddVec2(A, B) (Vec2){(A).x + (B).x, (A).y + (B).y}
#define SubVec2(A, B) (Vec2){(A).x - (B).x, (A).y - (B).y}
#define MulVec2(A, B) (Vec2){(A).x * (B).x, (A).y * (B).y}
#define DivVec2(A, B) (Vec2){(A).x / (B).x, (A).y / (B).y}
#define DotVec2(A, B) ((A).x * (B).x + (A).y * (B).y)

static inline Vec2 RotateVec2(Vec2 v, f32 angle){
    Vec2 n;
    n.x = v.x * cosf(angle) - v.y * sinf(angle);
    n.y = v.y * cosf(angle) + v.x * sinf(angle);
    return n;
}

typedef struct{
    f32 x, y, z;
}Vec3;

#define Vec3(X, Y, Z) (Vec3){.x = (X), .y = (Y), .z = (Z)}
#define AddVec3(A, B) (Vec3){(A).x + (B).x, (A).y + (B).y, (A).z + (B).z,}
#define SubVec3(A, B) (Vec3){(A).x - (B).x, (A).y - (B).y, (A).z - (B).z,}
#define MulVec3(A, B) (Vec3){(A).x * (B).x, (A).y * (B).y, (A).z * (B).z,}
#define DivVec3(A, B) (Vec3){(A).x / (B).x, (A).y / (B).y, (A).z / (B).z,}
#define DotVec3(A, B) ((A).x * (B).x + (A).y * (B).y + (A).z * (B).z)

typedef struct{
    f32 x, y, z, w;
}Vec4;

#define Vec4(X, Y, Z, W) (Vec4){.x = (X), .y = (Y), .z = (Z), .w = (W)}

typedef struct{
 f32 x, y;
 f32 w, h;
} Rect;

#define Rect(X, Y, W, H) (Rect){(X), (Y), (W), (H)}

static inline f32 approachf(f32 v, f32 step, f32 goal){
    if(v > goal) return MAX(v - step, goal);
    if(v < goal) return MIN(v + step, goal);
    return goal;
}

static inline i32 signi(i32 value){
    if(value > 0) return 1;
    if(value < 0) return -1;
    return 0;
}

static inline f32 signf(f32 value){
    if(value > 0.0f) return 1.0f;
    if(value < 0.0f) return -1.0f;
    return 0.0f;
}

static inline void set_zero(void *mem, size_t size){
    u8 *block = (u8*)mem;
    for(size_t i = 0; i < size; i++){
        block[i] = 0;
    }
}

static inline f32 length(f32 x, f32 y){
    return sqrtf(x * x + y * y);
}

static f32 dist(Vec2 a, Vec2 b){
    return length(a.x - b.x, a.y - b.y);
}

static inline f32 clampf(f32 v, f32 min, f32 max){
    if(v < min) return min;
    if(v > max) return max;
    return v;
}

static inline i32 clampi(i32 v, i32 min, i32 max){
    if(v < min) return min;
    if(v > max) return max;
    return v;
}

static inline void warpi(i32 *v, i32 min, i32 max){
    if(*v < min) *v = max;
    else if(*v > max) *v = min;
}


// ===================================================================
// Graphics
// ===================================================================

typedef union{
    u32 u;
    u8 channel[4];
    struct{
        u8 r, g, b, a;
    };
}Color;

typedef struct{
    i32 x, y;
    u8* buffer;
}Bitmap;

// ===================================================================
// RNGs
// ===================================================================

void set_seed(u64 seed);
u64 get_seed(void);
u64 random_u64(void);
i64 random_i64(void);
i32 random_u32(void);
i32 random_i32(void);
i32 random_in(i32 a, i32 b);
u32 random_n(u32 max);

#ifdef BASIC_IMPLEMENT
#include <intrin.h>

u64 _RNGSeed = 0;
u64 _RNGinitSeed = 0;

// from: https://rosettacode.org/wiki/Pseudo-random_numbers/Splitmix64
static u64 splitmix64(void){
    _RNGSeed += 0x9e3779b97f4a7c15;
    u64 z = _RNGSeed;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

void set_seed(u64 seed){
    assert(seed != 0);
    _RNGinitSeed = seed;
    _RNGSeed = seed;
    for(i32 i = 0; i < 16; i++) splitmix64();
}

u64 get_seed(void){
    return _RNGinitSeed;
}

u64 random_u64(void){
    if(!_RNGSeed) set_seed(__rdtsc());
    return splitmix64();
}

i64 random_i64(void){
    return (i64)(random_u64() & MAXi64);
}

i32 random_u32(void){
    return random_u64() & MAXu32;
}

i32 random_i32(void){
    return (i32)(random_u64() & MAXi32);
}

i32 random_in(i32 a, i32 b){
    i32 len = abs(a - b) + 1;
    return a + random_i32() % len;
}

u32 random_n(u32 exclusive_max){
    return random_u32() % exclusive_max;
}

#endif
#endif
