#ifndef PARTICLE_ENGINE_H
#define PARTICLE_ENGINE_H

#ifdef USE_GLM

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct Particle
{
    union
    {
        glm::vec2 pos;
        struct
        {
            float x, y;
        };
    };

    union
    {
        glm::vec2 scale;
        struct
        {
            float sx, sy; 
        };
    };

    union
    {
        glm::vec4 color;
        struct 
        {
            float r, g, b, a;
        };
    };
#if __x86_64__
    void* user;
    uintptr_t reserved;
#else
    void* user;
    uintptr_t reserved[3];
#endif
};

#else

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>

    typedef struct Particle
    {
        float x, y;
        float sx, sy;
        float r, g, b, a;
#if __x86_64__
        void* user;
        uintptr_t reserved;
#else
        void* user;
        uintptr_t reserved[3];
#endif
    } Particle;  

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*CloseFunc)(void);


    Particle* CreateEngine(int n, int w, int h
        , CloseFunc close = 0
        , int s = 12
        , float r = 0.0f, float g = 0.0f, float b = 0.0f
        );
    void CloseEngine();

#ifdef __cplusplus
}
#endif


#endif 
