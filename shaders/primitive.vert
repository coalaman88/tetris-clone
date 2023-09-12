#version 330
in vec2 pos;
in vec4 color;

uniform mat4 ident_matrix;
uniform mat4 trans_matrix;
out vec4 vertexColor;

void main()
{
    gl_Position = trans_matrix * ident_matrix * vec4(pos, -1.0, 1.0);
    vertexColor = color;
}
