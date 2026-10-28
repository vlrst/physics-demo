#include <iostream>
#include <SDL3/SDL.h>
//#include <math.h>
#include <cmath>


#define GRAVITY 9.81f // m/s^2 and shoutout AP Physics 1 lol
#define PI 3.141592653f


// static vars - NOTE THIS IS A GUESS, TESLA HAS NOT GIVEN OFFICIAL SPECS AS OF OCTOBER 16TH 2026 @ 3:31PM EST
static int counter = 0;
static const float MASS = 1900.f; // kg
//static const float LENGTH = 4.3f; // meters
//static const float HEIGHT = 1.2f; // meters

static const float LENGTH = 8.6f;
static const float HEIGHT = 2.4f;

static const int ISP = 70; // secs - thruster efficiency
static const int NOZ_MAX = 30000; // N, max force of one underside nozzle
static const int RAMP_RAD = 6; // meters, ramp radius


// vars for when the car hits the ground, it acts like a spring
static const float SPRING_K = 150000.f; // N/m, spring stiffness
static const float DAMPER_C = 12000.f; // N*s/m, how much bounce the car will soak up

static const float CONTACT_X[4] = {  1.4f, -1.4f,  2.0f, -2.0f };   // front wheel, rear wheel, front roof, rear roof
static const float CONTACT_Y[4] = { -0.6f, -0.6f,  0.6f,  0.6f };

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
Given that (Fx, Fy) is a force, and it is applied at (rx, ry) this is Torque = rx*Fy - ry*Fx



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

void kinematics(float initial_velocity, float acceleration, float time, float &final_velocity, float &delta_displacement) {
    // Vf = Vi + a(t)
	final_velocity = initial_velocity + (acceleration * time);
    // d = (1/2)(Vf+Vi) * t
	delta_displacement = 0.5f * (final_velocity+initial_velocity) * time; // 
}



/*

Physics time step - discrete increment of time used for solve math equations to capture rapid changes and fast moving objects


*/




int main() {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Initialization failed - %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window;
    SDL_Renderer *renderer;

    if (!SDL_CreateWindowAndRenderer("Physics Simulation", 1280, 720, 0, &window, &renderer)) return 1;
    SDL_SetRenderVSync(renderer, 1); // frame rate limit 


    const float TS = 1.f/ 240.f; // 240 steps per second 
    const float PIXELS_PER_METER = 14.f;
    const float GROUND_Y = 640.f;
    float car_y = 10.f;
	car_y = 40.f;
    float car_vel_y = 0.f; // vertical velocity
    float elapsed_time = 0.f;
    bool landed = false;


    float car_x = 0.f; // 
    float car_angle = 0.f; // rad. 0 = upright; counterclockwise positive, clockwise negative
    float car_spin = 2.f; // rad
    //float angular_accel = 0.f;
	float angular_accel = 5.0f;
    double leftover_time = 0.0;
    Uint64 last_clock = SDL_GetTicksNS();
    
/*
    private:
        int x=0;
*/







    bool run = true;
    while (run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) run = false;
        }


        Uint64 current_clock = SDL_GetTicksNS();
        double frame_duration = (current_clock-last_clock) * 1e-9;
        last_clock = current_clock;
        if (frame_duration >= 0.1) frame_duration = 0.1; // cap the frame time

        leftover_time += frame_duration;


        while (leftover_time >= TS) {
            if (!landed) {
                float vel_f, delta_d;
                kinematics(car_vel_y, -GRAVITY, TS, vel_f, delta_d);
                car_vel_y = vel_f;
                car_y += delta_d;

                // rotation physics
                float spin_f, delta_angle;
                kinematics(car_spin, angular_accel, TS, spin_f, delta_angle);
                car_spin = spin_f;
                car_angle += delta_angle;

                elapsed_time += TS;
				counter++;	
                if (car_y <= 0.f) {
                    car_y = 0.f;
                    landed = true;
                    std::cout << "LAND TIME: " << elapsed_time << "sec SPEED: " << car_vel_y << "m/s\n";
                    std::cout << "ANGLE : " << car_angle * 180.f/PI << " degrees\n"; // radians to degrees is radians * 180/pi
                } else {
					std::cout << "COUNTER: " << counter << std::endl;
					std::cout << "TIME (s): " << elapsed_time << " CURRENT VELOCITY (m/s): " << car_vel_y << std::endl;
				} 
            }
            leftover_time -= TS;
        }




        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 120, 120, 130, 255);
        SDL_RenderLine(renderer, 0, GROUND_Y, 1280, GROUND_Y);
        
        SDL_SetRenderDrawColor(renderer, 230, 40, 40, 255); // car is a box just cus
        

        // car
        float half_length = LENGTH / 2.f;
        float half_height = HEIGHT / 2.f;

        // 4 corners of car; first corner repeats to close the outline of the car
        float corner_x[5] = { -half_length, half_length, half_length, -half_length, -half_length };
        float corner_y[5] = { -half_height, -half_height, half_height, half_height, -half_height };
        SDL_FPoint outline[5];
        for (int i = 0; i < 5; i++) {
            float world_x = car_x + corner_x[i] * cosf(car_angle) - corner_y[i] * sinf(car_angle);
            float world_y = car_y + corner_x[i] * sinf(car_angle) + corner_y[i] * cosf(car_angle);
            outline[i].x = 640.f + world_x * PIXELS_PER_METER;
            outline[i].y = GROUND_Y - world_y * PIXELS_PER_METER;
        }

        SDL_RenderLines(renderer, outline, 5);

        float nose_x = car_x + half_length * cosf(car_angle);
        float nose_y = car_y + half_length * sinf(car_angle);
        SDL_RenderLine(renderer,
        
            640.f + car_x * PIXELS_PER_METER, GROUND_Y - car_y * PIXELS_PER_METER,
            640.f + nose_x + PIXELS_PER_METER, GROUND_Y - nose_y * PIXELS_PER_METER 
        
        );

        // line for debugging/seeing the direction the car is facing

        SDL_RenderPresent(renderer);
    }
    SDL_Quit();
    return 0;
}
