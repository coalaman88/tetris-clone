#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec4 color;

uniform mat4 ident_matrix;
uniform mat4 trans_matrix;
uniform mat3 texture_trans_matrix;
out vec4 vertexColor;
out vec2 texUV;

void main()
{
    gl_Position = trans_matrix * ident_matrix * vec4(pos, -1.0, 1.0);
    vertexColor = color;
    vec3 texture_uv = texture_trans_matrix * vec3(texCoord, 1);
    texUV = texture_uv.xy;
}
