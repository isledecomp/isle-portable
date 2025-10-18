#version 450
layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aColour;
layout(location = 0) out vec4 vColour;
void main() {
    gl_Position = aPosition;
    vColour = aColour;
}
