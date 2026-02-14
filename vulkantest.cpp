#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <stdexcept>
#include <iostream>

// If we want to be able to use Vulkan with SDL, we need to create a window with the appropriate flags. This macro 
// defines the flags we need to use when creating the window.
#define SDL_VULKAN_FLAGS (SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)

// Struct to hold Vulkan objects so they survive past initVulkan
struct VulkanState {
    vk::raii::Context context{};
    vk::raii::Instance instance{nullptr};
    vk::raii::SurfaceKHR surface{nullptr};
    vk::raii::PhysicalDevice physicalDevice{nullptr};
    vk::raii::Device device{nullptr};
    vk::raii::Queue graphicsQueue{nullptr};
};

// Opens a window
SDL_Window* initWindow() {
    // Initialize SDL, and draw a window with Vulkan support. Handle appropriate errors.
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        // If we can't initialize SDL, throw an error with the SDL error message
        throw std::runtime_error(SDL_GetError());

    // Create a Vulkan-capable window. Handle errors appropriately.
    SDL_Window* window = SDL_CreateWindow(
        "Vulkan",
        800,
        600,
        SDL_VULKAN_FLAGS
    );

    if (!window)
        // If we can't open a window, throw an error with the SDL error message
        throw std::runtime_error(SDL_GetError());

    return window;
}

// Initializes Vulkan and creates a Vulkan surface for the given window. Handles errors appropriately.
VulkanState initVulkan(SDL_Window* window) {
    VulkanState state;

    // ================================================== Vulkan Setup =================================================
    // RAII Context for Vulkan. Essentially a safer C++ manager for Vulkan
    state.context = vk::raii::Context{};

    // Tell Vulkan about our application.
    vk::ApplicationInfo appInfo{};
    appInfo.setPApplicationName("Minimal App")
           .setApplicationVersion(VK_MAKE_VERSION(1,0,0))
           .setPEngineName("No Engine")
           .setEngineVersion(VK_MAKE_VERSION(1,0,0))
           .setApiVersion(VK_API_VERSION_1_4);

    // ================================================ SDL Extensions =================================================
    // Get the list of Vulkan instance extensions required by SDL.
    Uint32 extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);  

    // Handle the case where we fail to get the extensions
    if (!sdlExts) throw std::runtime_error("Failed to get SDL extensions");

    // Store the extensions in a vector for easier use with Vulkan
    std::vector<const char*> extensions(sdlExts, sdlExts + extCount);

    // ================================================ Vulkan Instance ================================================
    // Describe the Vulkan instance we want to create. This tells the Vulkan instance about the application and 
    // the extensions we want to use. We will use the extensions provided by SDL to create a surface for our window.
    vk::InstanceCreateInfo instanceInfo{};
    instanceInfo.setPApplicationInfo(&appInfo)
                .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
                .setPpEnabledExtensionNames(extensions.data());

    // Create the Vulkan instance
    state.instance = vk::raii::Instance(state.context, instanceInfo);

    // ==================================================== Surface ====================================================
    // Create a Vulkan surface for our window
    VkSurfaceKHR rawSurface;

    // SDL_Vulkan_CreateSurface() is a boolean function that checks if the surface was created successfully.
    if (!SDL_Vulkan_CreateSurface(
            window,
            static_cast<VkInstance>(*state.instance),
            nullptr,
            &rawSurface))
    {   
        // If we fail to create a surface, throw an error with the SDL error message
        throw std::runtime_error(SDL_GetError());
    }

    // Wrap the raw Vulkan surface 
    state.surface = vk::raii::SurfaceKHR(state.instance, rawSurface);

    // ================================================ Physical Device ================================================
    // Enumerate through the available GPUs on the computer and pick the first one
    auto physicalDevices = state.instance.enumeratePhysicalDevices();

    // If the list of GPUs is empty, throw an error
    if (physicalDevices.empty())
        throw std::runtime_error("No GPUs found");

    // Move the first GPU into a vk::raii::PhysicalDevice object
    state.physicalDevice = physicalDevices.front();

    // ================================================ Queue Selection ================================================
    auto queueFamilies = state.physicalDevice.getQueueFamilyProperties();

    uint32_t graphicsFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        bool graphics = (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};

        bool present = state.physicalDevice.getSurfaceSupportKHR(i, *state.surface);

        if (graphics && present)
        {
            graphicsFamily = i;
            break;
        }
    }

    if (graphicsFamily == UINT32_MAX) throw std::runtime_error("No suitable queue");

    // ======================================= Vulkan Device and Queue Creation ========================================
    // Set the priority of the queue to 1.0. Vulkan expects an array of priorities, but since we only have one queue, we
    // can just use a single float. The priority is a value between 0.0 and 1.0, where 1.0 is the highest priority.
    float priority = 1.0f;

    // Describe the queue we want to create. We want one queue from the graphics family, with the highest priority.
    vk::DeviceQueueCreateInfo queueInfo{};
    queueInfo.setQueueFamilyIndex(graphicsFamily)
             .setQueueCount(1)
             .setPQueuePriorities(&priority);

    // Specify device extensions (needed for presenting)
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // Create the logical device. RAII will handle cleanup.
    vk::DeviceCreateInfo deviceInfo{};
    deviceInfo.setQueueCreateInfos(queueInfo)
              .setEnabledExtensionCount(1)
              .setPpEnabledExtensionNames(deviceExtensions);

    state.device = vk::raii::Device(state.physicalDevice, deviceInfo);

    // Get the graphics queue from the device. Since there is only one Queue in the family, we can just get the first 
    // one (index 0).
    state.graphicsQueue = state.device.getQueue(graphicsFamily, 0);

    return state;
}

// Main loop of the application. This is where we render frames and handle events
void mainLoop() {
    // =================================================== Main Loop ===================================================

    // Populate a boolean variable which controls if we are running or not. When we quit, this will be set to false.
    bool running = true;

    // SDL_Event object to hold event data. We will use this to poll for events in the main loop.
    SDL_Event e;

    // While the status of the application is running, poll for events. If we get a quit event, set running to false. 
    while (running)
    {   
        // Poll for events. SDL_PollEvent() is a boolean function that returns 1 if there is an event in the queue, and
        // 0 if there are no events. It also fills the SDL_Event structure with the event data. 
        while (SDL_PollEvent(&e))
        {   
            // If we get a quit event, set running to false (Like if the user clicks the X button on the window)
            if (e.type == SDL_EVENT_QUIT)
                running = false;
        }
        // Wait 16ms (62.5 fps). Replace with proper frame timing later to reduce stutter.
        SDL_Delay(16);
    }
}

// The RAII Vulkan wrapper automatically handles Vulkan cleanup, so we only need to destroy SDL resources here.
void cleanup(SDL_Window* window) {
    // ==================================================== Cleanup ====================================================
    // RAII handles Vulkan automatically

    SDL_DestroyWindow(window);
    SDL_Quit();
}

// This function dictates the flow of the program.
void run() {
    SDL_Window* window = initWindow();
    VulkanState vkState = initVulkan(window);
    mainLoop();
    cleanup(window);

}

int main()
{   
    // Try to run it
    try {
        run();
    } catch (const std::exception& e) {
        // If we can't run the application, print the error message and exit with a failure code
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
