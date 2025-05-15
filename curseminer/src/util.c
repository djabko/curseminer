#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "curseminer/util.h"
#include "curseminer/globals.h"
#include "curseminer/time.h"

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

    milliseconds_t seed = TIMER_NOW.usec;
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


/* Sprites Functions*/
GIF *g_gif = NULL;

// Can be used for jpeg too
typedef struct PNG {
    char *name;
    void *data;
    int width, height, stride;
} PNG;

typedef struct GIF {
    char *name;
    void *data;
    int width, height, layers, stride;
    int *delays;
} GIF;

static void load_gif(char *filename, GIF *gif) {
    size_t size;
    FILE *f = fopen(filename, "rb");

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    assert_log(0 < size, "Error: File '%s' is empty", filename);

    void *buf = malloc(size);

    size_t r = fread(buf, 1, size, f);

    assert_log(r == size,
            "Error: Failed to read file '%s', %lu bytes returned", filename, r);

    gif->name = filename;
    gif->data = stbi_load_gif_from_memory(buf, size, &gif->delays,
            &gif->width, &gif->height, &gif->layers, &gif->stride, 0);

    assert_log(gif->data, "Error: Failed to load '%s' as GIF", filename);

    free(buf);
}

static Spritesheet *spritesheet_alloc(char *name, int width, int height, int layers,
        int stride, milliseconds_t delay) {

    const size_t alloc_size =
        sizeof(Spritesheet) + layers * sizeof(SDL_Texture*);

    Spritesheet *sp = calloc(alloc_size, 1);

    assert_log(sp != NULL,
            "Error: failed to allocate %lu for spritesheet '%s'",
            alloc_size, name);

    strncpy(sp->name, name, NAME_MAX);
    sp->frames = (SDL_Texture**) (sp + 1);
    sp->width = width;
    sp->height = height;
    sp->layers = layers;
    sp->stride = stride;
    sp->delay = delay;

    return sp;
}

static SDL_Texture *spritesheet_init_frame(Spritesheet *sp, void *img_data,
        SDL_Renderer* renderer, int stride, int pitch) {

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
            img_data, sp->width, sp->height,
            stride * 8, pitch, g_bmr, g_bmg, g_bmb, g_bma);

    SDL_Texture *frame = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    assert_SDL(frame != NULL, "Failed to initialize SDL_Texture");

    return frame;
}

Spritesheet *load_spritesheet_raw(const char *path) {

}

static file_format_t check_file_format(const char *path) {
    bool exists = 0 == access(path, R_OK);

    if (exists) {
        FILE *f = fopen(path, "rb");

        static byte_t buf[] = {0, 0, 0, 0, 0};

        fread(buf, 1, 4, f);

        if      (0 == memcmp(buf, MAGIC_PNG,  4)) return FILE_FORMAT_PNG;
        else if (0 == memcmp(buf, MAGIC_GIF,  4)) return FILE_FORMAT_GIF;
        else if (0 == memcmp(buf, MAGIC_JPEG, 4)) return FILE_FORMAT_JPEG;
    }
    
    return FILE_FORMAT_INVALID;
}

// Make sure to free Spritesheet after use
static Spritesheet *load_spritesheet_from_file(SDL_Renderer *renderer, char *path) {
    Spritesheet *ss = NULL;
    file_format_t ff = check_file_format(path);

    if (ff == FILE_FORMAT_PNG || ff == FILE_FORMAT_JPEG) {
        PNG png;
        png.name = path;
        png.data = stbi_load(path, &png.width, &png.height, &png.stride, 0);
        ss = spritesheet_from_png(&png, renderer);

    } else if (ff == FILE_FORMAT_GIF) {
        GIF gif;
        load_gif(path, &gif);
        ss = spritesheet_from_gif(&gif, renderer);
    }

    return ss;
}


Spritesheet *load_spritesheet_png(PNG *png, SDL_Renderer *renderer) {
    const int pitch = png->width * png->stride;
    const int layers = 1;

    Spritesheet *sp = spritesheet_alloc(
            png->name, png->width, png->height, layers, png->stride, 0);

    sp->frames[0] = spritesheet_init_frame(
            sp, png->data, renderer, png->stride, pitch);

    return sp;
}

Spritesheet *load_spritesheet_gif(GIF *gif, SDL_Renderer *renderer) {
    const int pitch = gif->width * gif->stride;

    Spritesheet *sp = spritesheet_alloc(
            gif->name, gif->width, gif->height, gif->layers, gif->stride,
            gif->delays[0]);

    void *data = gif->data;

    for (int i = 0; i < gif->layers; i++) {
        sp->frames[i] = spritesheet_init_frame(
                sp, data, renderer, gif->stride, pitch);

        data += gif->width * gif->height * gif->stride;
    }

    return sp;
}

