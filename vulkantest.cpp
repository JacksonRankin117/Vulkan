#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

#define SDL_INCLUDE_VULKAN
#define SDL_VULKAN_FLAGS (SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)
#define VK_CHECK(x) do { VkResult err = x; if (err != VK_SUCCESS) throw std::runtime_error("Vulkan error at " #x); } while(0)

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
private:
    // Store the SDL window as a member variable for cleanup and future use
    SDL_Window* window = nullptr;

    // VkInstance will also be stored as a member variable for cleanup and future use
    VkInstance instance;

    // Create the SDL window with Vulkan support and handle errors appropriately
    void initWindow() {
        // Initialize SDL. Throw an error if it fails, and quit.
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
            throw std::runtime_error("Failed to initialize SDL");
        }

        // Create a window
        window = SDL_CreateWindow(
            "SDL3 Window",    // Title
            800,              // Pixel Width
            600,              // Pixel Height
            SDL_VULKAN_FLAGS  // Use the appropriate flags for Vulkan support and window properties
        );

        // Check if creating the window succeeded. If it failed, print the error and quit.
        if (!window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
            throw std::runtime_error("Failed to create SDL window");
            SDL_Quit();
        }
    }

    // Initialize Vulkan
    void initVulkan() {
        createInstance();
    }

    // Create the Vulkan instance
    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "ComputeTest";
        appInfo.apiVersion = VK_API_VERSION_1_3;

        // Get SDL Extensions
        Uint32 sdlExtCount = 0;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

        // Check if we got any extensions from SDL. If not, throw an error.
        if (!sdlExtensions || sdlExtCount == 0) {
            throw std::runtime_error("Failed to get Vulkan extensions from SDL3");
        }
        
        // Copy SDL extensions into a vector and add the portability extension for macOS/MoltenVK
        std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

        // Modern Vulkan requirement for portability (macOS/MoltenVK)
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

        // Create the Vulkan instance with the required extensions
        VkInstanceCreateInfo instanceCI{};
        instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCI.pApplicationInfo = &appInfo;
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCI.ppEnabledExtensionNames = extensions.data();
        
        // Enable portability bit
        instanceCI.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        // Create the Vulkan instance and check for errors
        VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));
    }

    void mainLoop() {
        // Main loop: Poll for events and handle them. If we receive a quit event, set running to false to exit the loop.
        bool running = true;
        SDL_Event event;

        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }

            SDL_Delay(16); // Draws frames with a delay of 16ms, or 62.5 fps equivalent
        }
    }

    // Destroy the window, if any, and quit SDL
    void cleanup() {
        if (instance) {
            vkDestroyInstance(instance, nullptr);
        }
        if (window) { 
            SDL_DestroyWindow(window); 
        }
        SDL_Quit();
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}