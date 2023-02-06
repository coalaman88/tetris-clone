#version 140
in vec2 pos;
in vec4 color;
in vec2 texCoord;

uniform mat4 ident_matrix;
uniform mat4 trans_matrix;
out vec4 vertexColor;
out vec2 texUV;

void main()
{
    gl_Position = trans_matrix * ident_matrix * vec4(pos, -1.0, 1.0);
    vertexColor = color;
    texUV = texCoord;
}
