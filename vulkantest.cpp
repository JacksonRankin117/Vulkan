#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <stdexcept>
#include <iostream>

#define f

int main()
{
    // ---------------- SDL ----------------

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        throw std::runtime_error(SDL_GetError());

    SDL_Window* window = SDL_CreateWindow(
        "Vulkan",
        800,
        600,
        SDL_WINDOW_VULKAN
    );

    if (!window)
        throw std::runtime_error(SDL_GetError());


    // ---------------- Vulkan Context ----------------

    vk::raii::Context context;


    // ---------------- Application Info ----------------

    vk::ApplicationInfo appInfo{};
    appInfo.setPApplicationName("Minimal App")
           .setApplicationVersion(VK_MAKE_VERSION(1,0,0))
           .setPEngineName("No Engine")
           .setEngineVersion(VK_MAKE_VERSION(1,0,0))
           .setApiVersion(VK_MAKE_API_VERSION(0, 1, 4, 0));


    // ---------------- SDL Required Extensions ----------------

    Uint32 extCount = 0;
    const char* const* sdlExts =
        SDL_Vulkan_GetInstanceExtensions(&extCount);

    if (!sdlExts)
        throw std::runtime_error("Failed to get SDL extensions");

    std::vector<const char*> extensions(
        sdlExts,
        sdlExts + extCount
    );


    // ---------------- Instance ----------------

    vk::InstanceCreateInfo instanceInfo{};
    instanceInfo.setPApplicationInfo(&appInfo)
                .setEnabledExtensionCount(
                    static_cast<uint32_t>(extensions.size()))
                .setPpEnabledExtensionNames(
                    extensions.data());

    vk::raii::Instance instance(context, instanceInfo);


    // ---------------- Surface ----------------

    VkSurfaceKHR rawSurface;

    if (!SDL_Vulkan_CreateSurface(
            window,
            static_cast<VkInstance>(*instance),
            nullptr,
            &rawSurface))
    {
        throw std::runtime_error("Surface creation failed");
    }

    vk::raii::SurfaceKHR surface(instance, rawSurface);


    // ---------------- Physical Device ----------------

    auto physicalDevices = instance.enumeratePhysicalDevices();

    if (physicalDevices.empty())
        throw std::runtime_error("No GPUs found");

    vk::raii::PhysicalDevice physicalDevice =
        std::move(physicalDevices.front());


    // ---------------- Queue Selection ----------------

    auto queueFamilies =
        physicalDevice.getQueueFamilyProperties();

    uint32_t graphicsFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        bool graphics =
            (queueFamilies[i].queueFlags &
            vk::QueueFlagBits::eGraphics)
            != vk::QueueFlags{};

        bool present =
            physicalDevice.getSurfaceSupportKHR(i, *surface);

        if (graphics && present)
        {
            graphicsFamily = i;
            break;
        }
    }

    if (graphicsFamily == UINT32_MAX)
        throw std::runtime_error("No suitable queue");


    // ---------------- Logical Device ----------------

    float priority = 1.0f;

    vk::DeviceQueueCreateInfo queueInfo{};
    queueInfo.setQueueFamilyIndex(graphicsFamily)
             .setQueueCount(1)
             .setPQueuePriorities(&priority);

    vk::DeviceCreateInfo deviceInfo{};
    deviceInfo.setQueueCreateInfos(queueInfo);

    vk::raii::Device device(
        physicalDevice,
        deviceInfo
    );

    vk::raii::Queue graphicsQueue{ nullptr };
    graphicsQueue = device.getQueue(graphicsFamily, 0);


    // ---------------- Minimal Loop ----------------

    bool running = true;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
        }

        SDL_Delay(16);
    }


    // ---------------- Cleanup ----------------
    // RAII handles Vulkan automatically

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
