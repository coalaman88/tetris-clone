#pragma once

// TODO some random functions are in basic.h

typedef struct{
	f32 x, y;
}Vertex2;

typedef struct{
  i32 x, y;
}Vec2i;

typedef struct{
  i32 x, y;
  i32 w, h;
}Recti;

typedef Vertex2 Vec2f;

#define Vec2i(X, Y) (Vec2i){.x = (X), .y = (Y)}

f32 random_unit();
f32 random_scalar();
Vec2 random_vector();
Vec2 normalize_v2(Vec2 vec);

//=============================================
// Vector Math
//=============================================

// Vec2

static inline Vec2 add_v2(Vec2 a, Vec2 b){
  a.x += b.x;
  a.y += b.y;
  return a;
}

static inline Vec2 sub_v2(Vec2 a, Vec2 b){
  a.x -= b.x;
  a.y -= b.y;
  return a;
}

static inline Vec2 mul_v2(Vec2 a, Vec2 b){
  a.x *= b.x;
  a.y *= b.y;
  return a;
}

static inline Vec2 scale_v2(Vec2 v, f32 s){
  v.x *= s;
  v.y *= s;
  return v;
}

static inline f32 dot_v2(Vec2 a, Vec2 b){
  a.x *= b.x;
  a.y *= b.y;
  return a.x + b.y;
}

// Vec3

static inline Vec3 add_v3(Vec3 a, Vec3 b){
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

static inline Vec3 sub_v3(Vec3 a, Vec3 b){
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}

static inline Vec3 mul_v3(Vec3 a, Vec3 b){
  a.x *= b.x;
  a.y *= b.y;
  a.z *= b.z;
  return a;
}

static inline Vec3 scale_v3(Vec3 v, f32 s){
  v.x *= s;
  v.y *= s;
  v.z *= s;
  return v;
}

static inline f32 dot_v3(Vec3 a, Vec3 b){
  a.x *= b.x;
  a.y *= b.y;
  a.z *= b.z;
  return a.x + b.y;
}


static inline void rotation_matrix_2x2(f32 *matrix_2x2, f32 angle){
  f32 cos = cosf(angle);
  f32 sin = sinf(angle);
  matrix_2x2[0] =  cos;
  matrix_2x2[1] = -sin;
  matrix_2x2[2] =  sin;
  matrix_2x2[3] =  cos;
}

static inline Vec2 transform_v2(f32 *matrix_2x2, Vec2 v){
  Vec2 u;
  u.x = matrix_2x2[0] * v.x + matrix_2x2[1] * v.y;
  u.y = matrix_2x2[2] * v.x + matrix_2x2[3] * v.y;
  return u;
}

Vec2 lerp_v2(Vec2 start, Vec2 end, f32 t); // TODO make other lerp functions and simplify this one

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

static inline void warpf(f32 *v, f32 min, f32 max){
  if(*v < min) *v = max;
  else if(*v > max) *v = min;
}

static inline void warpi(i32 *v, i32 min, i32 max){
  if(*v < min) *v = max;
  else if(*v > max) *v = min;
}

Vec2 aim_v2(Vec2 aimer, Vec2 target);

// Misc
b32 parse_int(const char *s, i32 *out);

#ifdef UTIL_IMPLEMENT

f32 random_unit(){
  const i32 presicion = 100000;
  return (f32)random_in(-presicion, presicion) / presicion;
}

f32 random_scalar(){
  const i32 presicion = 100000;
  return (f32)random_n(presicion) / presicion;
}

Vec2 random_vector(){
  i32 sign = random_n(1) * 2 - 1;
  f32 x = (f32)random_in(-1000, 1000) / 1000.0f;
  f32 y = sqrtf(1.0f - (x * x)) * (f32)sign;
  return Vec2(x, y);
}

Vec2 lerp_v2(Vec2 start, Vec2 end, f32 t){
  Vec2 v = scale_v2(sub_v2(end, start), t);
  return add_v2(start, v);
}

Vec2 aim_v2(Vec2 aimer, Vec2 target){
  Vec2 v_size = sub_v2(target, aimer);
  f32 len = length(v_size.x, v_size.y);
  f32 cos = len > 0.0f? v_size.x / len : 0.0f; // trig formula
  f32 sin = len > 0.0f? v_size.y / len : 0.0f;

  return Vec2(cos, sin);
}

Vec2 normalize_v2(Vec2 vec){
  f32 norm = length(vec.x, vec.y);
  if(norm != 0){
    vec.x /= norm;
    vec.y /= norm;
  }
  return vec;
}

b32 parse_int(const char *s, i32 *out){
  *out = 0;
  if(!s || *s == 0) return false;

  do{
    char c = *s++;
    if(c >= '0' && c <= '9'){
      *out *= 10;
      *out += c - '0';
    } else return false;
  }while(*s);
  return true;
}

#undef UTIL_IMPLEMENT
#endif
