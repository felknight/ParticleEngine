/*
* @Author: Felknight
* @Date:   2017-02-25 23:25:50
* @Last Modified by:   Felknight
* @Last Modified time: 2017-02-26 04:54:19
*/

#include <iostream>
#include <random>

#include <unistd.h>
#include <sys/time.h>

#define USE_GLM
#include <glm/glm.hpp>
#include <ParticleEngine.h>

using namespace std;
using namespace glm;

static mt19937 engine(12131878998);
static uniform_real_distribution<float> dist;

float Random(float max, float min = 0.0f)
{
    return min + (dist(engine) * (max - min));
}

//Physical properties of particles
struct Phy
{
    glm::vec2 s; //StartPosition
    glm::vec2 v; //Velocity
    float t; //Time relative to itself (Quantum Relativity?)
};

constexpr int N = 500;

Phy phy[N];

struct EParticle
{
    union
    {
        vec2 pos;
        struct
        {
            float x, y;
        };
    };

    union
    {
        vec2 scale;
        struct
        {
            float sx, sy; 
        };
    };

    union
    {
        vec4 color;
        struct 
        {
            float r, g, b, a;
        };
    };

    Phy* p;
    uintptr_t reserved;
};

int runProgram = 1;

void Close()
{
    runProgram = 0;
}

vec2 lastv;


int main() 
{
    float r = 12;
    float W = 800;
    float H = 600;
    float speedFactor = 0.2f;
    float aheadvalue = 0.01f;

    //Created draw engine
    EParticle* p = (EParticle*)CreateEngine(N, W, H, Close);
    if (p == nullptr)
    {
        fprintf(stderr, "Could not create the engine\n");
        return 0;
    }

    for (int i = 0; i < N; i++)
    {
        p[i].scale = vec2(r, r);
        p[i].p = &phy[i];
        p[i].p->v = vec2(Random(1.0f, -1.0f), Random(1.0f, -1.0f));
        p[i].p->v = normalize(p[i].p->v) * (float)i * speedFactor;

        p[i].p->s = vec2(Random(800 - r , r), Random(600 -r, r ));
        
        for (int j = 0; j < i; j++)
        {
            vec2 dist = p[i].p->s - p[j].p->s;

            while (dot(dist, dist) <= r * r)
            {
                p[i].p->s = vec2(Random(800 - r , r), Random(600 -r, r ));
                dist = p[i].p->s - p[j].p->s;
            }
        }
        p[i].p->t = 0.0f;

    }


    p[0].p->v.y += 1.0f;

    //Async update from draw operations
    timeval val;
    gettimeofday(&val, nullptr);
    double start = (double)val.tv_sec + (double)val.tv_usec / 1000000.0f;

    
    int run = 0;
    while(runProgram)
    {
        double t;
        gettimeofday(&val, nullptr);
        t = (double)val.tv_sec + (double)val.tv_usec / 1000000.0f;
        double dt = t - start;
        start = t;
        //dt = 0.0016f;

        for (int i = 0; i < N; i++)
        {
            p[i].p->t += dt;
            p[i].pos = p[i].p->s + (p[i].p->v * p[i].p->t);
        }

        vec2 df = p[0].p->v - lastv;
        if ( dot(df , df) > 0.1f)
        {
            //printf("%f,%f (%f) \n", p[0].p->v.x, p[0].p->v.y, p[0].p->t);
            lastv = p[0].p->v;
        }

        for (int _i = 0; _i < N; _i++)
        {
            for (int _j = _i+1; _j < N; _j++)
            {
                vec2& i = p[_i].pos;
                vec2& j = p[_j].pos;

                vec2& vi = p[_i].p->v;
                vec2& vj = p[_j].p->v;

                vec2 diff = j - i;
                if ( dot(diff, diff) <= r * r)
                {
                    float len = (length(vi) + length(vj)) / 2.0f;

                    vec2 norm1 = normalize(i - j);
                    vec2 ref1 = normalize(reflect(vi, norm1));

                    vec2 norm2 = normalize(j - i);
                    vec2 ref2 = normalize(reflect(vj, norm2));

                    vi = ref1 * len;
                    vj = ref2 * len;

                    p[_i].p->s = i;
                    p[_j].p->s = j;
                    p[_i].p->t = 0.0f;
                    p[_j].p->t = 0.0f;
                }
            }
        }

        for (int i = 0; i < N; i++)
        {
            if (p[i].pos.x >= W - r/2.0f)
            {
                vec2 newv = glm::reflect(p[i].p->v, vec2(-1.0f, 0.0f));
                p[i].p->s = p[i].pos;
                //p[i].p->s.x -= 1.0f;
                p[i].p->v = newv;
                p[i].p->t = 0.0f;
            }

            if (p[i].pos.x <= r/2.0f)
            {
                vec2 newv = glm::reflect(p[i].p->v, vec2(1.0f, 0.0f));
                p[i].p->s = p[i].pos + 1.0f;
                //p[i].p->s.x += 1.0f;
                p[i].p->v = newv;
                p[i].p->t = 0.0f;
            }

            if (p[i].pos.y >= H - r/2.0f)
            {
                vec2 newv = glm::reflect(p[i].p->v, vec2(0.0f, -1.0f));
                p[i].p->s = p[i].pos;
                //p[i].p->s.y -= 1.0f;
                p[i].p->v = newv;
                p[i].p->t = 0.0f;
            }

            if (p[i].pos.y <= r/2.0f)
            {
                vec2 newv = glm::reflect(p[i].p->v, vec2(0.0f, 1.0f));
                p[i].p->s = p[i].pos;
                //p[i].p->s.y += 1.0f;
                p[i].p->v = newv;
                p[i].p->t = 0.0f;
            }
        }       

        /*float magg = dot(p[0].p->v, p[0].p->v);
        float max = magg, min = magg;

        for (int i = 0; i < N; i++)
        {   
            magg = dot(p[i].p->v, p[i].p->v);
            if (magg > max)
                max = magg;
            if (magg < min)
                min = magg;
        } */

        for (int i = 0; i < N; i++)
        {
            float magg = length(p[i].p->v);
            
            if (magg > N * speedFactor * 0.7f )
                p[i].color = vec4( magg / (N * speedFactor), magg / (N * speedFactor * 1.6f)
                    , magg / (N * speedFactor * 2.0f), 1.6f );
            else
                p[i].color = vec4( magg / (N * speedFactor), 0.0, 0.0 , 1.0f );
        }

        usleep(160);
        run++;
    }

    CloseEngine();

    return 0;
}
