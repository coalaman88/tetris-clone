#version 330 core
in vec4 vertexColor;
in vec2 texUV;
out vec4 FragColor;

uniform sampler2D sample_tex;

void main(){
  vec4 sample = texture(sample_tex, texUV);
  gl_FragColor = sample * vertexColor;
}
