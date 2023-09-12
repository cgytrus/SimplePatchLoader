#version 120
#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_separate_shader_objects : require

attribute layout(location = 0) vec2 aPosition;
attribute layout(location = 1) vec2 aTexCoords;

varying vec2 TexCoords;

void main() {
    gl_Position = vec4(aPosition.x, aPosition.y, 0.0, 1.0);
    TexCoords = aTexCoords;
}
