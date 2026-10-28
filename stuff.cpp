// roadster_simple.cpp  --  the Tesla Roadster ramp flip, kept as simple as possible
//
// Build:  g++ roadster_simple.cpp -o roadster_simple $(pkg-config --cflags --libs sdl3)
// Keys:   R = restart     ESC = quit

#include <iostream>
#include <cmath>
#include <cstdio>
#include <SDL3/SDL.h>

#define GRAVITY 9.81f   // m/s^2 and shoutout AP Physics 1 lol
#define PI 3.141592653f
#define MPH 2.23694f    // m/s -> mph

// NOTE THIS IS A GUESS, TESLA HAS NOT GIVEN OFFICIAL SPECS AS OF OCTOBER 16TH 2026
static const float MASS          = 1900.f;    // kg
static const float LENGTH        = 4.3f;      // meters
static const float HEIGHT        = 1.2f;      // meters
static const float RAMP_RAD      = 6.f;       // meters, ramp radius
static const float RAMP_SPEED    = 20.f;      // m/s, speed the car holds while on the ramp
static const float DRIVE_ACCEL   = 25.f;      // m/s^2, speeding up before the ramp
static const float SPIN_NOZ_MAX  = 10000.f;   // N, max force of each nozzle in the spinning pair
static const float LIFT_NOZ_MAX  = 30000.f;   // N, max force of each underside nozzle
static const float TURN_SPRING   = 9.f;       // how hard the thrusters pull the car toward the target angle
static const float TURN_DAMPER   = 6.f;       // how hard they fight the spin so it doesn't overshoot
static const float SPRING_K      = 150000.f;  // N/m, suspension stiffness
static const float DAMPER_C      = 12000.f;   // N*s/m, removes bounce
static const float FRICTION      = 0.8f;      // tire grip on the ground

// how much resistance the car has to being spun
static const float inertia = MASS * (LENGTH*LENGTH + HEIGHT*HEIGHT) / 12.f;

// the 4 points on the car that can touch the ground (in the car's own frame)
// front wheel, rear wheel, front roof, rear roof
static const float CONTACT_X[4] = {  1.4f, -1.4f,  2.0f, -2.0f };
static const float CONTACT_Y[4] = { -0.6f, -0.6f,  0.6f,  0.6f };

/*
KINEMATICS FORMULAS (my formula sheet)
1. avg_vel   = delta_d / time
2. avg_accel = delta_vel / time
3. vel_f = vel_i + accel*time
4. delta_d = 1/2 (vel_f + vel_i) * time
5. delta_d = vel_i*time + 1/2 accel time^2
6. vel_f^2 = vel_i^2 + 2 accel delta_d
Rotation uses the same formulas: delta_d -> delta_angle, vel -> spin, accel -> angular_accel
*/

// formulas 3 and 4, for one small time step
void kinematics(float initial_velocity, float acceleration, float time,
                float &final_velocity, float &delta_displacement) {
    final_velocity     = initial_velocity + acceleration * time;                 // formula 3
    delta_displacement = 0.5f * (final_velocity + initial_velocity) * time;      // formula 4
}

// keeps a number between low and high
float clamp_float(float value, float low, float high) {
    if (value < low)  return low;
    if (value > high) return high;
    return value;
}

// ---------- drawing helpers ----------
static const float PIXELS_PER_METER = 14.f;
static const float GROUND_Y = 640.f;

float to_screen_x(float world_x, float camera_x) { return 640.f + (world_x - camera_x) * PIXELS_PER_METER; }
float to_screen_y(float world_y)                  { return GROUND_Y - world_y * PIXELS_PER_METER; }   // screen y points down

void draw_car(SDL_Renderer *renderer, float car_x, float car_y, float car_angle, float camera_x) {
    float half_length = LENGTH / 2.f;
    float half_height = HEIGHT / 2.f;

    // 4 corners of the car; the first corner repeats to close the outline
    float corner_x[5] = { -half_length, half_length, half_length, -half_length, -half_length };
    float corner_y[5] = { -half_height, -half_height, half_height, half_height, -half_height };

    SDL_FPoint outline[5];
    for (int i = 0; i < 5; i++) {
        float world_x = car_x + corner_x[i] * cosf(car_angle) - corner_y[i] * sinf(car_angle);
        float world_y = car_y + corner_x[i] * sinf(car_angle) + corner_y[i] * cosf(car_angle);
        outline[i].x = to_screen_x(world_x, camera_x);
        outline[i].y = to_screen_y(world_y);
    }
    SDL_SetRenderDrawColor(renderer, 230, 40, 40, 255);
    SDL_RenderLines(renderer, outline, 5);

    // line from the center to the front, so you can see which way the car faces
    float nose_x = car_x + half_length * cosf(car_angle);
    float nose_y = car_y + half_length * sinf(car_angle);
    SDL_RenderLine(renderer, to_screen_x(car_x, camera_x), to_screen_y(car_y),
                             to_screen_x(nose_x, camera_x), to_screen_y(nose_y));

    // wheels
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < 2; i++) {
        float wheel_x = car_x + CONTACT_X[i] * cosf(car_angle) - CONTACT_Y[i] * sinf(car_angle);
        float wheel_y = car_y + CONTACT_X[i] * sinf(car_angle) + CONTACT_Y[i] * cosf(car_angle);
        SDL_FRect wheel = { to_screen_x(wheel_x, camera_x) - 4.f, to_screen_y(wheel_y) - 4.f, 8.f, 8.f };
        SDL_RenderFillRect(renderer, &wheel);
    }
}

// a short line coming out of a nozzle. local_x/local_y = where the nozzle is on the car (in half-lengths/half-heights)
// push = +1 if the nozzle pushes the car up (so the flame points down), -1 if it pushes the car down
void draw_flame(SDL_Renderer *renderer, float car_x, float car_y, float car_angle, float camera_x,
                float local_x, float local_y, float push, float strength) {
    if (strength < 0.02f) return;
    float offset_x = local_x * LENGTH / 2.f;
    float offset_y = local_y * HEIGHT / 2.f;
    float nozzle_x = car_x + offset_x * cosf(car_angle) - offset_y * sinf(car_angle);
    float nozzle_y = car_y + offset_x * sinf(car_angle) + offset_y * cosf(car_angle);
    float flame_x = nozzle_x + sinf(car_angle) * push * strength * 2.f;     // flame points opposite the push
    float flame_y = nozzle_y - cosf(car_angle) * push * strength * 2.f;
    SDL_SetRenderDrawColor(renderer, 255, 200, 80, 255);
    SDL_RenderLine(renderer, to_screen_x(nozzle_x, camera_x), to_screen_y(nozzle_y),
                             to_screen_x(flame_x, camera_x),  to_screen_y(flame_y));
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Initialization failed - %s", SDL_GetError());
        return 1;
    }
    SDL_Window *window;
    SDL_Renderer *renderer;
    if (!SDL_CreateWindowAndRenderer("Tesla Roadster Physics Sim", 1280, 720, 0, &window, &renderer)) return 1;
    SDL_SetRenderVSync(renderer, 1);       // frame rate limit (must come AFTER the renderer exists)

    const float TS = 1.f / 240.f;          // 240 physics steps per second

    // car variables (the real starting values get set by the restart block below)
    float car_x = 0.f, car_y = 0.f, car_angle = 0.f;
    float car_vel_x = 0.f, car_vel_y = 0.f, car_spin = 0.f;
    float ramp_angle = 0.f;                // how far around the ramp we are
    float target_angle = 0.f;              // the angle the spin thrusters are aiming for
    float spin_force = 0.f;                // N, force of each nozzle in the spinning pair
    float lift_force = 0.f;                // N, total force of the two underside nozzles
    float elapsed_time = 0.f;
    int   phase = 0;                       // 0 = driving, 1 = on the ramp, 2 = flying
    bool  touching_ground = false;
    bool  had_touchdown = false;
    int   step_count = 0;

    float camera_x = 0.f;
    bool  restart = true;                  // true = (re)set every variable at the top of the next frame

    double leftover_time = 0.0;
    Uint64 last_clock = SDL_GetTicksNS();

    bool run = true;
    while (run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) run = false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) run = false;
                if (event.key.key == SDLK_R) restart = true;
            }
        }

        if (restart) {
            car_x = -14.f;  car_y = 0.6f;  car_angle = 0.f;
            car_vel_x = 0.f;  car_vel_y = 0.f;  car_spin = 0.f;
            ramp_angle = 0.f;  target_angle = 0.f;  elapsed_time = 0.f;
            spin_force = 0.f;  lift_force = 0.f;
            phase = 0;  touching_ground = false;  had_touchdown = false;  step_count = 0;
            camera_x = car_x;
            restart = false;
        }

        Uint64 current_clock = SDL_GetTicksNS();
        double frame_duration = (current_clock - last_clock) * 1e-9;
        last_clock = current_clock;
        if (frame_duration > 0.1) frame_duration = 0.1;      // cap the frame time
        leftover_time += frame_duration;

        while (leftover_time >= TS) {
            float vel_f, delta_d;

            if (phase == 0) {
                // ---------- DRIVING toward the ramp ----------
                kinematics(car_vel_x, DRIVE_ACCEL, TS, vel_f, delta_d);
                car_vel_x = vel_f;
                if (car_vel_x > RAMP_SPEED) car_vel_x = RAMP_SPEED;     // don't go faster than the ramp speed
                car_x += delta_d;

                if (car_x >= 0.f) phase = 1;                            // reached the ramp

            } else if (phase == 1) {
                // ---------- ON THE RAMP (fans hold the car on, so it just follows the circle) ----------
                ramp_angle += (RAMP_SPEED / RAMP_RAD) * TS;             // angle = (speed / radius) * time

                float center_radius = RAMP_RAD - 0.6f;                  // the car's center is 0.6 m inside the track
                car_x = center_radius * sinf(ramp_angle);
                car_y = RAMP_RAD - center_radius * cosf(ramp_angle);
                car_angle = ramp_angle;

                if (ramp_angle >= PI) {
                    // top of the ramp, upside down: let go. The car keeps the speed and the spin it had.
                    phase = 2;
                    car_vel_x = -RAMP_SPEED * center_radius / RAMP_RAD;
                    car_vel_y = 0.f;
                    car_spin  = RAMP_SPEED / RAMP_RAD;                  // spin = speed / radius
                    target_angle = 2.f * PI;                            // rotate the rest of the way around to upright
                }

            } else {
                // ---------- FLYING ----------
                float angle_error = target_angle - car_angle;

                // 1) the spinning pair of thrusters (two nozzles on opposite corners, so they spin the car without pushing it around)
                //    a spring pulls the angle toward the target, a damper stops it from overshooting
                spin_force = 0.f;
                if (!touching_ground) {
                    float wanted_angular_accel = TURN_SPRING * angle_error - TURN_DAMPER * car_spin;
                    spin_force = clamp_float(wanted_angular_accel * inertia / LENGTH, -SPIN_NOZ_MAX, SPIN_NOZ_MAX);
                }

                // 2) the landing thrusters (both underside nozzles). Formula 6 with vel_f = 0:
                //    0 = vel_i^2 + 2*accel*delta_d  ->  accel = vel_i^2 / (2 * height left)
                lift_force = 0.f;
                float height_left = car_y - 0.6f;
                if (!touching_ground && fabsf(angle_error) < 0.5f && car_vel_y < -3.f && height_left > 0.1f) {
                    float accel_needed = (car_vel_y * car_vel_y) / (2.f * height_left);
                    lift_force = clamp_float(MASS * (accel_needed + GRAVITY), 0.f, 2.f * LIFT_NOZ_MAX);
                }

                float force_x = 0.f, force_y = 0.f, torque = 0.f;
                force_x += -sinf(car_angle) * lift_force;      // lift pushes along the car's "up" direction
                force_y +=  cosf(car_angle) * lift_force;
                torque  += spin_force * LENGTH;                // 2 nozzles, each pushing at a distance of LENGTH/2

                // 3) ground contact: a spring and a damper at each contact point
                touching_ground = false;
                for (int i = 0; i < 4; i++) {
                    // where this point is, relative to the car's center (rotation formula)
                    float offset_x = CONTACT_X[i] * cosf(car_angle) - CONTACT_Y[i] * sinf(car_angle);
                    float offset_y = CONTACT_X[i] * sinf(car_angle) + CONTACT_Y[i] * cosf(car_angle);

                    float point_height = car_y + offset_y;
                    if (point_height >= 0.f) continue;                 // not touching the ground

                    // how fast this point is moving (the car's velocity plus its spin)
                    float point_vel_x = car_vel_x - car_spin * offset_y;
                    float point_vel_y = car_vel_y + car_spin * offset_x;

                    float normal_force = SPRING_K * (-point_height) - DAMPER_C * point_vel_y;
                    if (normal_force < 0.f) normal_force = 0.f;        // the ground can push but never pull

                    float friction_force = -FRICTION * normal_force * clamp_float(point_vel_x, -1.f, 1.f);   // grip, slows the car down

                    force_x += friction_force;
                    force_y += normal_force;
                    torque  += offset_x * normal_force - offset_y * friction_force;     // torque = rx*Fy - ry*Fx
                    touching_ground = true;
                }

                // 4) turn forces into motion (formulas 3 and 4)
                float angular_accel = torque / inertia;                 // alpha = torque / inertia
                kinematics(car_vel_x, force_x / MASS,             TS, vel_f, delta_d);  car_vel_x = vel_f;  car_x     += delta_d;
                kinematics(car_vel_y, force_y / MASS - GRAVITY,   TS, vel_f, delta_d);  car_vel_y = vel_f;  car_y     += delta_d;
                kinematics(car_spin,  angular_accel,              TS, vel_f, delta_d);  car_spin  = vel_f;  car_angle += delta_d;

                if (touching_ground && !had_touchdown) {
                    had_touchdown = true;
                    std::cout << "TOUCHDOWN at " << elapsed_time << " s, vertical speed " << car_vel_y
                              << " m/s, angle " << car_angle * 180.f / PI << " degrees\n";
                }
            }

            elapsed_time += TS;
            step_count++;
            if (step_count % 120 == 0)
                std::cout << "t=" << elapsed_time << " phase=" << phase << " x=" << car_x << " y=" << car_y
                          << " angle=" << car_angle * 180.f / PI << " deg\n";
            leftover_time -= TS;
        }

        camera_x += (car_x - camera_x) * 0.08f;       // camera follows the car

        // ---------- drawing ----------
        SDL_SetRenderDrawColor(renderer, 15, 15, 20, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 120, 120, 130, 255);          // ground
        SDL_RenderLine(renderer, 0, GROUND_Y, 1280, GROUND_Y);

        SDL_FPoint ramp[41];                                           // the curved ramp
        for (int i = 0; i <= 40; i++) {
            float angle_on_ramp = PI * i / 40.f;
            ramp[i].x = to_screen_x(RAMP_RAD * sinf(angle_on_ramp), camera_x);
            ramp[i].y = to_screen_y(RAMP_RAD - RAMP_RAD * cosf(angle_on_ramp));
        }
        SDL_SetRenderDrawColor(renderer, 90, 160, 255, 255);
        SDL_RenderLines(renderer, ramp, 41);

        draw_car(renderer, car_x, car_y, car_angle, camera_x);

        if (phase == 2) {
            float spin_strength = fabsf(spin_force) / SPIN_NOZ_MAX;
            if (spin_force > 0.f) {            // counterclockwise pair: front-underside and rear-roof
                draw_flame(renderer, car_x, car_y, car_angle, camera_x,  1.f, -1.f,  1.f, spin_strength);
                draw_flame(renderer, car_x, car_y, car_angle, camera_x, -1.f,  1.f, -1.f, spin_strength);
            } else {                           // clockwise pair: rear-underside and front-roof
                draw_flame(renderer, car_x, car_y, car_angle, camera_x, -1.f, -1.f,  1.f, spin_strength);
                draw_flame(renderer, car_x, car_y, car_angle, camera_x,  1.f,  1.f, -1.f, spin_strength);
            }
            float lift_strength = lift_force / (2.f * LIFT_NOZ_MAX);
            draw_flame(renderer, car_x, car_y, car_angle, camera_x,  1.f, -1.f, 1.f, lift_strength);
            draw_flame(renderer, car_x, car_y, car_angle, camera_x, -1.f, -1.f, 1.f, lift_strength);
        }

        // text on screen
        char text[160];
        float speed = sqrtf(car_vel_x * car_vel_x + car_vel_y * car_vel_y);
        const char *phase_name = "driving";
        if (phase == 1) phase_name = "on the ramp";
        if (phase == 2) phase_name = "flying";
        snprintf(text, sizeof text, "%s   t=%.2fs   speed=%.0f mph   angle=%.0f deg", phase_name, elapsed_time, speed * MPH, car_angle * 180.f / PI);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, 10, 10, text);
        SDL_RenderDebugText(renderer, 10, 24, "R = restart   ESC = quit");

        SDL_RenderPresent(renderer);
    }

    SDL_Quit();
    return 0;
}
