#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <math.h>

#include "timer.h"

/* Distance Functions */
static inline double euc_dist(double x1, double y1, double x2, double y2) {
    return floor( sqrt( pow(x2 - x1, 2) + pow(y2 - y1, 2) ) );
}

static inline int man_dist(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

static inline int che_dist(int x1, int y1, int x2, int y2) {
    return round( max( abs(x1 - x2), abs(y1 - y2) ) );
}


/* Interpolation Functions */
static inline double lerp(double a, double b, double t) {
    return a * (1 - t) + b * t;
}

static inline double smoothstep(double t) {
    return 3 * t * t - 2 * t * t * t;
}

static inline double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}


/* Vector Functions */
typedef struct Vec2 {
    double x, y;
} Vec2;

static inline double vec2_dot(double x1, double y1, double x2, double y2) {
    return x1 * x2 + y1 * y2;
}


/* Noise Functions */
typedef struct NoiseLattice {
    int count, dimensions, length;
    double (*smoothing_func)(double);
    Vec2 *gradients;
} NoiseLattice;

// For 1D length must be equal to count, for 2D sqrt(count), for 3D cuberoot(count), etc...
static NoiseLattice *noise_init(int count, int dimensions, int length, double (*smoothing_func)(double)) {
    static int resolution = 100000;
    
    NoiseLattice *noise = calloc(sizeof(NoiseLattice) + sizeof(Vec2) * count, 1);
    Vec2 *gradients = (Vec2*) (noise+1);

    srand(TIMER_NOW.tv_usec);
    for (int i=0; i<count; i++) {
        int r = rand() % (resolution*2);
        double random = r - resolution;

        gradients[i].x = random / resolution;
        gradients[i].y = random / resolution;
    }
    
    noise->count = count;
    noise->dimensions = dimensions;
    noise->length = length;
    noise->gradients = gradients;
    noise->smoothing_func = smoothing_func;

    return noise;
}

static double value_noise_1D(NoiseLattice *lattice, double x) {
    /* 1. Index the position lattice
     * 2. Multiply position by surrounding points
     * 3. Interpolate the result
     */

    Vec2 *gradients = lattice->gradients;

    int low = (int) x;
    double t = lattice->smoothing_func(x - low);

    return lerp(gradients[low].x, gradients[low+1].x, t);
}

static double value_noise_2D(NoiseLattice *lattice, double x, double y) {
    /* 1. Index the position lattice
     * 2. Multiply position by each corner
     * 3. Interpolate the results
     */

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

static double perlin_noise_1D(NoiseLattice *lattice, double x) {
    Vec2 *gradients = lattice->gradients;

    int low = (int) x;
    int high = low + 1;

    double g0 = gradients[low].x > 0 ? 1.0 : -1.0;
    double g1 = gradients[high].x > 0 ? 1.0 : -1.0;

    double t = lattice->smoothing_func(x - low);

    double re = 2.0 * lerp(g0 * (x - low), g1 * (high - x), t);
    return re;
}

static double perlin_noise_2D(NoiseLattice *lattice, double x, double y) {
    Vec2 *gradients = lattice->gradients;

    int i_x = (int) x;
    int i_y = (int) y;
    int _i_x = (int) x % (lattice->length - 1);
    int _i_y = (int) y % (lattice->length - 1);

    Vec2 g0 = gradients[(_i_y + 0) * lattice->length + _i_x + 0];
    Vec2 g1 = gradients[(_i_y + 0) * lattice->length + _i_x + 1];
    Vec2 g2 = gradients[(_i_y + 1) * lattice->length + _i_x + 0];
    Vec2 g3 = gradients[(_i_y + 1) * lattice->length + _i_x + 1];

    double d0 = vec2_dot(g0.x, g0.y, x - g0.x, y - g0.y);
    double d1 = vec2_dot(g1.x, g1.y, g1.x - x, y - g1.y);
    double d2 = vec2_dot(g2.x, g2.y, x - g2.x, g2.y - y);
    double d3 = vec2_dot(g3.x, g3.y, g3.x - x, g3.y - y);

    double t_x = lattice->smoothing_func(x - i_x);
    double t_y = lattice->smoothing_func(y - i_y);

    double re = 2.0 * lerp(
            lerp(d0, d1, t_x),
            lerp(d2, d3, t_x),
            t_y
            );

    return re;
}

static inline void noise_free(NoiseLattice *noise) {
    free(noise);
}

#endif
