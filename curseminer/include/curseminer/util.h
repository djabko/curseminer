#ifndef UTIL_H
#define UTIL_H

/* Distance Functions */
double euc_dist(double x1, double y1, double x2, double y2);
int man_dist(int x1, int y1, int x2, int y2);
int che_dist(int x1, int y1, int x2, int y2);

/* Interpolation Functions */
double lerp(double a, double b, double t);
double smoothstep(double t);
double fade(double t);

/* Vector Functions */
typedef struct Vec2 {
    double x, y;
} Vec2;

double vec2_dot(double x1, double y1, double x2, double y2);

/* Noise Functions */
typedef struct NoiseLattice {
    int count, dimensions, length;
    double (*smoothing_func)(double);
    Vec2 *gradients;
} NoiseLattice;

NoiseLattice *noise_init(int count, int dimensions, int length, double (*smoothing_func)(double));
void noise_free(NoiseLattice *noise);
double value_noise_1D(NoiseLattice *lattice, double x);
double value_noise_2D(NoiseLattice *lattice, double x, double y);
double perlin_noise_1D(NoiseLattice *lattice, double x);
double perlin_noise_2D(NoiseLattice *lattice, double x, double y);
double perlin_noise_2D_var(NoiseLattice *lattice, double x, double y);

/* Sprite Functions */
#define SPRITE_MAX 256

typedef struct Spritesheet {
    char name[NAME_MAX];
    void **frames;
    int width, height, layers, stride;
    milliseconds_t delay;
} Spritesheet;


Spritesheet *load_spritesheet_png(const char *path);
Spritesheet *load_spritesheet_gif(const char *path);
Spritesheet *load_spritesheet_raw(const char *path);

#endif
