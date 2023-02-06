#version 140
in vec4 vertexColor;
in vec2 texUV;
out vec4 FragColor;

//uniform sampler2D sample_tex;

uniform sampler2D sample_tex;
uniform int my_index;

uniform int mode;
uniform vec2 screen;
uniform vec2 mouse_pos;

/*
void default_render(){
  vec4 sample = texture(sample_tex[index], texUV);

  vec3 fade = vec3(0.0, 1.0, 0.01);
  if(texUV.x > 0.5) sample *= vec4(fade * texUV.x, 1.0);
  gl_FragColor = sample * vertexColor;
}

void test_render(){
  vec4 sample = texture(sample_tex, texUV);
  //if(sample.a < 0.5) discard;
  gl_FragColor = sample * vertexColor;
}

void circle_render(){
  vec4 sample = vec4(1.0, 0, 0, 1.0);//texture(sample_tex, texUV);
  vec2 coord = gl_FragCoord.xy / screen;
  vec2 square = texUV - vec2(0.5, 0.5);
  square *= square;
  float radious = sqrt(square.x + square.y);
  //if(radious > 0.5) sample.w = 0;
  //1.0 - pow(t, 1.0/3.0) / sin(0.6)
  float t = (radious - 0.4) / (1.0 - 0.4);
  float fade = radious >= 0.4? 0.0: 1.0;

  sample.w *= fade;
  gl_FragColor = sample;
}

void full_screen_render(){

  vec4 sample = texture(sample_tex[index], texUV);
  vec2 coord = gl_FragCoord.xy;
  vec2 mouse = vec2(mouse_pos.x, screen. y- mouse_pos.y );

  float dist = length(coord - mouse);
  if(dist < 30) gl_FragColor = vec4(0.0, 1.0, 1.0, 1.0);
  else gl_FragColor = sample;

}

*/

void main(){
  vec4 sample = texture(sample_tex, texUV);
  gl_FragColor = sample * vertexColor;
}
