#version 150

in vec2 a_position;
in vec2 a_texCoord;

out vec2 TexCoords;

void main() {
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    TexCoords = a_texCoord;
}
