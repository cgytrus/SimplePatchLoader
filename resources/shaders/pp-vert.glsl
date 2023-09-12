#version 330 compatibility

in layout(location = 0) vec2 aPosition;
in layout(location = 1) vec2 aTexCoord;

out vec2 TexCoords;

void main() {
    gl_Position = vec4(aPosition.x, aPosition.y, 0.0, 1.0);
    TexCoords = aTexCoord;
}
