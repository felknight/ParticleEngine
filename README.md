# ParticleEngine
Simple to use, engine, implemented with Vulkan that renders 2D Circles of different color for simulations

##Screenshot of usage:
The red dots carry energy, the more bright the more energetic, over time the whole
system ends up with average energy.

![Example of usage screenshot](ExampleThermodynamics.png?raw=true "Thermodynamics Example")
![Example of usage screenshot](ExampleThermodynamics2.png?raw=true "Thermodynamics Example Final Stage")

##Example of usage:

```C

Particle* p = CreateEngine(100, 800, 600);

while(1)
{
    for (int i = 0; i < 100; i++)
    //Update particles
        p[i].x = funcx();
        p[i].x = funcy();
}

CloseEngine();


```

Create an engine with 100 particles in a window of 800 x 600

Thanks to Vulkan design you don't have to tell explicitly when particle position, color, scale have changed.
You can asume that every 16ms for a 60Hz monitor the driver will lookup at the memory and draw the particles
to the screen.

Particle structure contains the position, scale, color and a user pointer to aditional data used for the simulation.


