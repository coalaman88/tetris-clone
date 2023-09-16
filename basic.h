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
        u8 a, b, g, r;
    };
}Color;

#define Color(R, G, B, A) {.channel = {(A)*255, (R)*255, (G)*255, (B)*255}}

typedef struct{
    i32 x, y;
    u8* buffer;
}Bitmap;

// ===================================================================
// RNGs
// ===================================================================

#define RNGSEED32 1548112359 // Those are just randomaly typed numbers
#define RNGSEED64 2469558614013255971

extern u32 RNGseed;
extern u64 RNGseed64;

extern u32 Random(void);
extern u64 Random64(void);
extern i32 random_in(i32 a, i32 b);
extern u32 random_n(u32 max);

#ifdef BASIC_IMPLEMENT

u32 RNGseed   = RNGSEED32;
u64 RNGseed64 = RNGSEED64;

/* xorshift64s, variant A_1(12,25,27) with multiplier M_32 from line 3 of table 5 */
u32 Random(void) {
    u64 x = RNGseed64;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    RNGseed64 = x;
    return (u32)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

i32 random_in(i32 a, i32 b){
    i32 len = abs(a - b) + 1;
    return a + Random() % len;
}

u32 random_n(u32 max){
    return Random() % (max + 1);
}

u64 Random64(void){
    u64 z = (RNGseed64 += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

#endif
#endif
