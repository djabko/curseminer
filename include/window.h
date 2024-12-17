#ifndef WINDOW_HEADER
#define WINDOW_HEADER

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow* w;
    uint32_t width, height;
    char* title;
    GLFWmonitor* monitor;
    GLFWwindow* share;
} WindowContext;

int window_init();
int window_loop();
void window_free();

#endif
