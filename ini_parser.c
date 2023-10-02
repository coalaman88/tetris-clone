#include "basic.h"

static char* read_entire_file(const char* filename, int* file_size){

  FILE* fp = fopen(filename, "rb");

  if (fp == NULL) {
    fprintf(stderr, "Unable to open %s\n", filename);
    fclose(fp);
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  *file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char* file_contents = (char*)malloc(*file_size+1);
  if (file_contents == NULL) {
    fprintf(stderr, "Memory error: unable to allocate %d bytes\n", *file_size+1);
    return NULL;
  }

  if (fread(file_contents, *file_size, 1, fp) != 1 ){
    fprintf(stderr, "Unable to read content of %s\n", filename);
    fclose(fp);
    free(file_contents);
    return NULL;
  }

  fclose(fp);
  *(file_contents + *file_size) = 0;

  return file_contents;
}

b32 is_letter(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

b32 is_digit(char c){
    return c >= '0' && c <= '9';
}

b32 is_alpha_numeric(char c){
    return is_letter(c) || is_digit(c);
}

b32 is_space(char c){
    return c == ' ' | c == '\n' | c == '\r' | c == '\t';
}

typedef struct{
    i32 size;
    const char *data;
}String;

#define static_string(s) (String){.data = (s), .size = sizeof(s) - 1}
#define str_and_size(s) (s), sizeof(s) - 1


struct{
    String tokens[100];
    i32 count;
}TokenBuffer = {0};

void add_token(String t){
    assert(TokenBuffer.count < array_size(TokenBuffer.tokens));
    TokenBuffer.tokens[TokenBuffer.count++] = t;
}

void print_string(String s){
    i32 i = 0;
    while(i < s.size) putchar(s.data[i++]);
}

void eat_space(const char *str, i32 size, i32 *i){
    while(*i < size && is_space(str[*i]))
        *i += 1;
}

void reverse_eat_space(const char *str, i32 offset, i32 *i){
    while(*i >= offset && is_space(str[*i]))
    *i -= 1;
}

i32 parse_integer(const char *str, i32 size, i64 *integer){
    i32 i = 0;
    i32 sign = 1;
    u64 number = 0;

    if(i < size && str[i] == '-'){
        i++;
        sign = -1;
        eat_space(str, size, &i);
    }

    while(i < size && is_digit(str[i])){
        number *= 10;
        number += str[i++] - '0';
    }

    *integer = sign * number;
    return i;
}

b32 try_parse_integer(const char *str, i32 size, i64 *integer){
    b32 result = true;
    if(size <=0 || parse_integer(str, size, integer) != size){
        result = false;
        *integer = 0;
    }
    return result;
}

i32 parse_real(const char *str, i32 size, f64 *real){
    i64 integer;
    f64 decimal = 0;
    f64 sign;
    
    i32 i = parse_integer(str, size, &integer);
    if(i > 0 && integer == 0){
        sign = -1.0;
    } else {
        sign = integer >= 0? 1.0 : -1.0f;
    }

    if(str[i] == '.'){
        i++;
        f64 pow = 1;
        while(i < size && is_digit(str[i])){
            pow *= 10;
            decimal += (f64)(str[i++] - '0') / pow;
        }
    }

    *real = (f64)integer + decimal * sign;
    return i;
}

b32 try_parse_real(const char *str, i32 size, f64 *real){
    b32 result = true;
    if(size <=0 || parse_real(str, size, real) != size){
        result = false;
        *real = 0;
    }
    return result;
}

void test(void); // @remove me

void main(void){
    i32 size;
    u8 *data = read_entire_file("config.ini", &size);

    i32 i = 0;
    while(i < size){
        while(is_space(data[i])) i++;

        if(data[i] == '#'){
            do i++; while(i < size && data[i] != '\n');
            i++;
            continue;
        }

        if(data[i] == '='){
            String t;
            t.data = data + i++;
            t.size = 1;
            add_token(t);
            continue;
        }

        if(is_alpha_numeric(data[i]) || data[i] == '.' || data[i] == '_' || data[i] == '@'){
            i32 start = i++;
            while(i < size && (is_alpha_numeric(data[i]) || data[i] == '.' || data[i] == '_')){
                i++;
            }

            String t;
            t.data = data + start;
            t.size = i - start;
            add_token(t);
            continue;
        }

        printf("[%c]\n", data[i]);
        assert(false);
    }

    for(i32 i = 0; i < TokenBuffer.count; i++){
        String t = TokenBuffer.tokens[i];
        putchar('\'');print_string(t); putchar('\'');
        putchar('\n');
    }

    test();

    printf("exited\n");
}

void test(void){
        {
        i64 value; b32 result;
        result = try_parse_integer(str_and_size("1244"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == 1244);

        result = try_parse_integer(str_and_size("-34"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -34);

        result = try_parse_integer(str_and_size(" 42   "), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size(" 12.44"), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size("1244"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == 1244);

        result = try_parse_integer(str_and_size("-12"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -12);

        result = try_parse_integer(str_and_size("-     2"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -2);

        result = try_parse_integer(str_and_size("34"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == 34);

        result = try_parse_integer(str_and_size("--12"), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size("1-2"), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size("12-"), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size("-199023"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -199023);

        result = try_parse_integer(str_and_size("24  -12"), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);

        result = try_parse_integer(str_and_size("-763"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -763);

        result = try_parse_integer(str_and_size("-71"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == -71);

        result = try_parse_integer(str_and_size("1"), &value);
        printf("result: %lld\n", value);
        assert(result);
        assert(value == 1);

        result = try_parse_integer(str_and_size(""), &value);
        printf("result: %lld\n", value);
        assert(!result);
        assert(value == 0);
    }

    printf("real\n");
    {
        f64 value = 0; b32 result;
        
        result = try_parse_real(str_and_size(""), &value);
        printf("result: %f\n", value);
        assert(!result);
        assert(value == 0);

        
        result = try_parse_real(str_and_size("-2"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == -2);

        result = try_parse_real(str_and_size("- 24"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == -24);

        result = try_parse_real(str_and_size("3.5"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == 3.5);

        result = try_parse_real(str_and_size("3.234"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == 3.234);

        result = try_parse_real(str_and_size("-3.234"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == -3.234);

        result = try_parse_real(str_and_size("00.2324"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(abs(value - 0.2324) == 0);

        result = try_parse_real(str_and_size(".5"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == .5);

        result = try_parse_real(str_and_size("-.5"), &value);
        printf("result: %f\n", value);
        assert(result);
        assert(value == -.5);
    }
}