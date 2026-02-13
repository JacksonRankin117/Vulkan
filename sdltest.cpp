#include <SDL3/SDL.h>
#include <iostream>

int main(int argc, char* argv[])
{   
    // Initialize SDL. Throw an error if it fails, and quit.
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow(
        "SDL3 Window",  // Title
        800,            // Pixel Width
        600,            // Pixel Height
        0               // No flags
    );

    // Check if creating the window succeeded. If it failed, print the error and quit.
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    // If we got here, SDL was initialized and the window was created successfully. We can now enter the main loop.
    bool running = true;
    SDL_Event event;

    // Main loop: Poll for events and handle them. If we receive a quit event, set running to false to exit the loop.
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        SDL_Delay(16); // Draws frames with a delay of 16ms, or 62.5 fps equivalent
    }

    // Clean up and quit SDL
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
