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
    return t * t * (3 - 2 * t);
}

static inline double fade(double t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}


/* Noise functions */
typedef struct NoiseLattice {
    int count, dimensions, length;
    double *gradients;
} NoiseLattice;

// For 1D length must be equal to count, for 2D sqrt(count), for 3D cuberoot(count), etc...
static NoiseLattice *noise_init(int count, int dimensions, int length) {
    static int resolution = 100000;
    
    srand(TIMER_NOW.tv_usec);

    NoiseLattice *noise = calloc(sizeof(NoiseLattice) + sizeof(double) * count, 1);
    double *gradients = (double*) (noise+1);

    for (int i=0; i<count; i++) {
        gradients[i] = fabs((double) (rand() % resolution) / resolution);
    }
    
    noise->count = count;
    noise->dimensions = dimensions;
    noise->length = length;
    noise->gradients = gradients;

    return noise;
}

static double value_noise_1D(NoiseLattice *lattice, double x, double (*smoothing_func)(double)) {
    /* 1. Index the position lattice
     * 2. Multiply position by surrounding points
     * 3. Interpolate the result
     */

    double *gradients = lattice->gradients;

    int low = (int) x;
    double t = smoothing_func(x - low);

    return lerp(gradients[low], gradients[low+1], t);
}

static double value_noise_2D(NoiseLattice *lattice, double x, double y, double (*smoothing_func)(double)) {
    /* 1. Index the position lattice
     * 2. Multiply position by each corner
     * 3. Interpolate the results
     */

    double *gradients = lattice->gradients;
    int i_x = (int) x;
    int i_y = (int) y;
    int _i_x = i_x % (lattice->length - 1);
    int _i_y = i_y % (lattice->length - 1);
    
    double tl = gradients[_i_x * lattice->length + _i_y];
    double tr = gradients[_i_x * lattice->length + _i_y + 1];
    double bl = gradients[(_i_x + 1) * lattice->length + _i_y];
    double br = gradients[(_i_x + 1) * lattice->length + _i_y + 1];

    double t_x = x - i_x;
    double t_y = y - i_y;

    return lerp(
            lerp(tl, tr, smoothing_func(t_x)),
            lerp(bl, br, smoothing_func(t_x)),
            t_y
            );
}

static inline void noise_free(NoiseLattice *noise) {
    free(noise);
}

#endif
