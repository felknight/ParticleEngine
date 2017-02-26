#version 450

in vec4 partColor;

layout (location = 0) out vec4 color;

void main()
{
    color = partColor;
}
