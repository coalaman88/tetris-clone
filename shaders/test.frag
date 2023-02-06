#version 140
in vec4 vertexColor;
in vec2 texUV;
out vec4 FragColor;

uniform sampler2D sample_tex;

uniform int mode;
uniform vec2 screen;
uniform vec2 mouse_pos;

void main(){
  vec4 sample = texture(sample_tex, texUV);
  gl_FragColor = sample * vertexColor;
}
