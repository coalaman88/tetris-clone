#version 330 core
in vec4 vertexColor;

void main(){
  vec4 color = vertexColor;
  gl_FragColor = color;
}
