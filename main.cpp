#include <iostream>
#include <SDL3/SDL.h>
//#include <math.h>
#include <cmath>


#define GRAVITY 9.81f // m/s^2 and shoutout AP Physics 1 lol
#define PI 3.141592653f

// static vars - NOTE THIS IS A GUESS, TESLA HAS NOT GIVEN OFFICIAL SPECS AS OF OCTOBER 16TH 2026 @ 3:31PM EST

static const float MASS = 1900.f; // kg
static const float LENGTH = 4.3f; // meters
static const float HEIGHT = 1.2f; // meters
static const int ISP = 70; // secs - thruster efficiency
static const int NOZ_MAX = 30000; // N, max force of one underside nozzle
static const int RAMP_RAD = 6; // meters, ramp radius


float inertia = MASS * (LENGTH*LENGTH + HEIGHT*HEIGHT) / 12; // this is how much resistance the car has to being spun



/* 

KINETMATICS FORMULAS



avg_vel = delta d / time
avg_accel = delta v / time

vel_f = vel_i + accel*time

delta disp = 1/2(vel_f+vel_i)*time
delta disp = vel_i*time + (1/2)accel*(time^2)
vel_f^2 = vel_i^2 + 2(accel)(delta disp)

*/

/*


 

Newton's second law says Force = Mass * Acceleration
Rewrite this as Acceleration = Force/Mass


Newton's second law for rotation states Net Torque = Intertia * Angular Acceleration
Rewrite this as Angular Acceleration = Net Torque / Intertia

2D Torque
Given that (Fx, Fy) is a force, and it is applied at (rx, ry) this is 
Torque = rx*Fy - ry*Fx



HOW TO TURN ACCELERATION INTO MOTION

Chain 
    force -> acceleration -> velocity -> position

Update Velocity
vy += ay* dt (this is )

*/


/*

Rotating point from angle
x' = x·cosθ − y·sinθ
y' = x·sinθ + y·cosθ

*/


static float vel_f;
static float vel_i;
static float avg_v;
static float a;
static float avg_a;
static float t;
static float d;

// Rotation Vars
static float d_theta; // Straight line is delta T, however rotation is Δθ (delta theta)
static float w; // Straight like is v but rotation is ω (this is not a 'w')
static float alpha; // Straight line is accel and rotation is α (alpha)

// & is lowkey mad tuff
void kinematics(float initial_velocity, float acceleration, float time, float &final_velocity, float &delta_displacement) {
    final_velocity = initial_velocity + acceleration * time;
    delta_displacement = 0.5f * (final_velocity+initial_velocity) * time; // 

}


int main() {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Initialization failed - %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window;
    SDL_Renderer *renderer;
    if (!SDL_CreateWindowAndRenderer("Tesla Roadster Physics Sim", 1280, 720, 0, &window, &renderer)) return 1;

    bool run = true;
    while (run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) run = false;
        }

        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
    SDL_Quit();
    return 0;
}