#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <math.h>

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
    int count;
    double *gradients;
} NoiseLattice;

static NoiseLattice *noise_init(int count) {
    static int resolution = 100000;
    NoiseLattice *noise = calloc(sizeof(NoiseLattice) + sizeof(double) * count, 1);
    double *gradients = (double*) (noise+1);

    for (int i=0; i<count; i++) {
        gradients[i] = fabs((double) (rand() % resolution) / resolution);
    }
    
    noise->count = count;
    noise->gradients = gradients;

    return noise;
}

static double value_noise_1D(NoiseLattice *lattice, double x) {
    double *gradients = lattice->gradients;

    int low = (int) x;
    double t = smoothstep(x - low);
    
    return lerp(gradients[low], gradients[low+1], t);
}

static inline void noise_free(NoiseLattice *noise) {
    free(noise);
}

#endif
