#define GLFW_INCLUDE_VULKAN

#include <stdio.h>
#include <string.h>

#include "window.h"
#include "globals.h"

struct Private {
    VkApplicationInfo appinfo;

    VkInstance instance;
    VkInstanceCreateInfo ici;

    VkPhysicalDevice gpu;
    VkDeviceQueueCreateInfo dqci;

    VkDevice device;
    VkQueue queue;
    VkDeviceCreateInfo dci;

    VkSurfaceKHR surface;
} pri;

void error_callback(int error, const char* description) {
    static const char* ansi_color_red = "\x1b[31m";
    static const char* ansi_clear = "\x1b[0m";

    fprintf(stderr, "%sError: %s%s\n", ansi_color_red, description, ansi_clear);
    glfwSetWindowShouldClose(GLOBALS.window.w, GLFW_TRUE);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    fprintf(stderr, "Key pressed: %d\t%d\t%d\t%d\n", key, scancode, action, mods);

    if (action == GLFW_RELEASE && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q))
        glfwSetWindowShouldClose(GLOBALS.window.w, GLFW_TRUE);
}

int gpu_init() {
    uint32_t count = 0;

    vkEnumeratePhysicalDevices(pri.instance, &count, NULL);
    if (count < 1) return -1;

    VkPhysicalDevice devices[count];
    vkEnumeratePhysicalDevices(pri.instance, &count, devices);
    pri.gpu = devices[0];

    uint32_t queuep_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pri.gpu, &queuep_count, NULL);

    VkQueueFamilyProperties queueps[ queuep_count ];
    vkGetPhysicalDeviceQueueFamilyProperties(pri.gpu, &queuep_count, queueps);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(pri.gpu, &features);

    float priority = 1.0f;
    pri.dqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    pri.dqci.queueFamilyIndex = queueps+0 - queueps;
    pri.dqci.queueCount = queuep_count;
    pri.dqci.pQueuePriorities = &priority;

    pri.dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    pri.dci.pQueueCreateInfos = &pri.dqci;
    pri.dci.queueCreateInfoCount = 1;
    pri.dci.pEnabledFeatures = &features;
    pri.dci.enabledExtensionCount = pri.ici.enabledExtensionCount;;
    pri.dci.ppEnabledExtensionNames = pri.ici.ppEnabledExtensionNames;
    pri.dci.enabledLayerCount = 0;
    pri.dci.queueCreateInfoCount = 1;
    pri.dci.pQueueCreateInfos = &pri.dqci;

    return 0;
}

int vulkan_init() {
    uint32_t available = 0;
    uint32_t enabled = 0;
    const char **_extensions;
    vkEnumerateInstanceExtensionProperties(NULL, &available, NULL);
    _extensions = glfwGetRequiredInstanceExtensions(&enabled);
    VkExtensionProperties extensionprops[available];
    vkEnumerateInstanceExtensionProperties(NULL, &available, extensionprops);

    fprintf(stderr, "%d/%d extensions available: {\n", enabled, available);
    for (int i=0; i<available; i++) fprintf(stderr, "'%s'%s", extensionprops[i].extensionName, i==enabled?"":", ");
    fprintf(stderr, "}\n\n");

    const char* extensions[enabled + 1];
    for (int i=0; i<enabled; i++) extensions[i] = _extensions[i];
    extensions[enabled++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    fprintf(stderr, "%d/%d extensions enabled: {\n", enabled, available);
    for (int i=0; i<enabled; i++) fprintf(stderr, "'%s'%s", extensions[i], i==enabled?"":", ");
    fprintf(stderr, "}\n\n");

    pri.appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    pri.appinfo.pApplicationName = GLOBALS.window.title;
    pri.appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    pri.appinfo.pEngineName = "No Engine";
    pri.appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    pri.appinfo.apiVersion = VK_API_VERSION_1_0;

    pri.ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    pri.ici.pApplicationInfo = &pri.appinfo;
    pri.ici.enabledExtensionCount = enabled - 1;
    pri.ici.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&pri.ici, NULL, &pri.instance);
    if (result != VK_SUCCESS || pri.instance == VK_NULL_HANDLE) {
        error_callback(-1, "Failed to create a Vulkan instance...");
        return -1;
    }

    if (gpu_init() != 0) error_callback(-1, "Failed to find a graphics card...");

    return 0;
}

int window_init() {
    if (!glfwInit()) error_callback(-1, "Failed to initialize GLFW...");
    if (glfwVulkanSupported() != GLFW_TRUE) error_callback(-1, "This GLFW setup does not support Vulkan...");
    if (vulkan_init() != 0) error_callback(-1, "Failed to initialize Vulkan...");;

    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLOBALS.window.w = glfwCreateWindow(GLOBALS.window.width, GLOBALS.window.height, GLOBALS.window.title, NULL, NULL);
    if (!GLOBALS.window.w) error_callback(-1, "Failed to create a window...");

    VkResult err = glfwCreateWindowSurface(pri.instance, GLOBALS.window.w, NULL, &pri.surface);
    if (err) error_callback(-1, "Failed to create Vulkan window surface on GLFW window...");

    glfwSetKeyCallback(GLOBALS.window.w, key_callback);

    return 0;
}

int window_loop() {
    //double time = glfwGetTime();

    int width, height;
    glfwGetFramebufferSize(GLOBALS.window.w, &width, &height);

    glfwPollEvents();

    return glfwWindowShouldClose(GLOBALS.window.w) ? -1 : 0;
}

void window_free() {
    vkDestroySurfaceKHR(pri.instance, pri.surface, NULL);
    vkDestroyDevice(pri.device, NULL);
    vkDestroyInstance(pri.instance, NULL);
    glfwDestroyWindow(GLOBALS.window.w);
    glfwTerminate();
}
