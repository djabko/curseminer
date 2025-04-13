#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "util.h"
#include "globals.h"
#include "timer.h"

/* Distance Functions */
double euc_dist(double x1, double y1, double x2, double y2) {
    return floor( sqrt( pow(x2 - x1, 2) + pow(y2 - y1, 2) ) );
}

int man_dist(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

int che_dist(int x1, int y1, int x2, int y2) {
    return round( max( abs(x1 - x2), abs(y1 - y2) ) );
}


/* Interpolation Functions */
double lerp(double a, double b, double t) {
    return a * (1 - t) + b * t;
}

double smoothstep(double t) {
    return 3 * t * t - 2 * t * t * t;
}

double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}


/* Vector Functions */
inline double vec2_dot(double x1, double y1, double x2, double y2) {
    return x1 * x2 + y1 * y2;
}


/* Noise Functions */

// For 1D length must be equal to count, for 2D sqrt(count), for 3D cuberoot(count), etc...
NoiseLattice *noise_init(int count, int dimensions, int length, double (*smoothing_func)(double)) {
    static int resolution = 100000;
    
    NoiseLattice *noise = calloc(sizeof(NoiseLattice) + sizeof(Vec2) * count, 1);
    Vec2 *gradients = (Vec2*) (noise+1);

    milliseconds_t seed = TIMER_NOW.tv_usec;
    seed = 259219;
    log_debug("Noise Lattice Seed: %lu", seed);
    srand(seed);
    for (int i=0; i<count; i++) {
        int r;
        double random;

        r = rand() % (resolution*2);
        random = r - resolution;
        gradients[i].x = random / resolution;
        
        r = rand() % (resolution*2);
        random = r - resolution;
        gradients[i].y = random / resolution;
    }
    
    noise->count = count;
    noise->dimensions = dimensions;
    noise->length = length;
    noise->gradients = gradients;
    noise->smoothing_func = smoothing_func;

    return noise;
}

double value_noise_1D(NoiseLattice *lattice, double x) {
    Vec2 *gradients = lattice->gradients;

    int low = (int) x;
    double t = lattice->smoothing_func(x - low);

    return lerp(gradients[low].x, gradients[low+1].x, t);
}

double value_noise_2D(NoiseLattice *lattice, double x, double y) {
    Vec2 *gradients = lattice->gradients;

    int i_x = (int) x;
    int i_y = (int) y;
    int _i_x = i_x % (lattice->length - 1);
    int _i_y = i_y % (lattice->length - 1);
    
    Vec2 tl = gradients[(_i_y + 0) * lattice->length + (_i_x + 0)];
    Vec2 tr = gradients[(_i_y + 0) * lattice->length + (_i_x + 1)];
    Vec2 bl = gradients[(_i_y + 1) * lattice->length + (_i_x + 0)];
    Vec2 br = gradients[(_i_y + 1) * lattice->length + (_i_x + 1)];

    double t_x = lattice->smoothing_func(x - i_x);
    double t_y = lattice->smoothing_func(y - i_y);

    double re = lerp(
            lerp(tl.x, tr.x, t_x),
            lerp(bl.x, br.x, t_x),
            t_y
            );

    return re;
}

double perlin_noise_1D(NoiseLattice *lattice, double x) {
    Vec2 *gradients = lattice->gradients;

    int low = (int) x;
    int high = low + 1;

    double g0 = gradients[low].x > 0 ? 1.0 : -1.0;
    double g1 = gradients[high].x > 0 ? 1.0 : -1.0;

    double t = lattice->smoothing_func(x - low);

    double re = 2.0 * lerp(g0 * (x - low), g1 * (high - x), t);
    return re;
}

double perlin_noise_2D(NoiseLattice *lattice, double x, double y) {
    Vec2 *gradients = lattice->gradients;

    int _i_x = ((int) x) % (lattice->length - 1);
    int _i_y = ((int) y )% (lattice->length - 1);

    int g0x = _i_x + 0;
    int g0y = _i_y + 0;
    int g1x = _i_x + 1;
    int g1y = _i_y + 0;
    int g2x = _i_x + 0;
    int g2y = _i_y + 1;
    int g3x = _i_x + 1;
    int g3y = _i_y + 1;

    Vec2 g0 = gradients[g0y * lattice->length + g0x];
    Vec2 g1 = gradients[g1y * lattice->length + g1x];
    Vec2 g2 = gradients[g2y * lattice->length + g2x];
    Vec2 g3 = gradients[g3y * lattice->length + g3x];

    double d0 = vec2_dot(g0.x, g0.y, x - g0x, y - g0y);
    double d1 = vec2_dot(g1.x, g1.y, g1x - x, y - g1y);
    double d2 = vec2_dot(g2.x, g2.y, x - g2x, g2y - y);
    double d3 = vec2_dot(g3.x, g3.y, g3x - x, g3y - y);

    double t_x = lattice->smoothing_func(x - (int) x);
    double t_y = lattice->smoothing_func(y - (int) y);

    double re = lerp(
            lerp(d0, d1, t_x),
            lerp(d2, d3, t_x),
            t_y
            );

    return re * 2.0;
}

double perlin_noise_2D_var(NoiseLattice *lattice, double x, double y) {
    Vec2 *gradients = lattice->gradients;

    int _i_x = ((int) x)% (lattice->length - 1);
    int _i_y = ((int) y) % (lattice->length - 1);

    Vec2 g0 = gradients[(_i_y + 0) * lattice->length + _i_x + 0];
    Vec2 g1 = gradients[(_i_y + 0) * lattice->length + _i_x + 1];
    Vec2 g2 = gradients[(_i_y + 1) * lattice->length + _i_x + 0];
    Vec2 g3 = gradients[(_i_y + 1) * lattice->length + _i_x + 1];

    double d0 = vec2_dot(g0.x, g0.y, x - g0.x, y - g0.y);
    double d1 = vec2_dot(g1.x, g1.y, g1.x - x, y - g1.y);
    double d2 = vec2_dot(g2.x, g2.y, x - g2.x, g2.y - y);
    double d3 = vec2_dot(g3.x, g3.y, g3.x - x, g3.y - y);

    double t_x = lattice->smoothing_func(x - (int) x);
    double t_y = lattice->smoothing_func(y - (int) y);

    double re = lerp(
            lerp(d0, d1, t_x),
            lerp(d2, d3, t_x),
            t_y
            );

    return re;
}

void noise_free(NoiseLattice *noise) {
    free(noise);
}
