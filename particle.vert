#version 450

layout (location = 0) in vec2 vert;

struct Particle
{
    vec2 pos;
    vec2 scale;
    vec4 color;
    ivec2 user; 
};

layout (binding = 0, std140) buffer Particles
{
    layout(offset = 0) float w;
    layout(offset = 4) float h;
    int reserved1;
    int reserved2;
    layout(offset = 16) Particle p[];
};


out vec4 partColor;

void main()
{
    mat3 tmat = mat3
    (
        p[gl_InstanceIndex].scale.x / w, 0.0, 0.0
        ,0.0, p[gl_InstanceIndex].scale.y / h, 0.0
        ,-1.0 + p[gl_InstanceIndex].pos.x * 2.0 / w
        ,-1.0 + p[gl_InstanceIndex].pos.y * 2.0 / h
        , 1.0
    );

    vec3 tvert = tmat * vec3(vert, 1.0);
    gl_Position = vec4(tvert.xy, 0.0, 1.0); 
    partColor = p[gl_InstanceIndex].color;
}
