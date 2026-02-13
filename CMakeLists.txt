cmake_minimum_required(VERSION 3.20)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 20)
find_package(Vulkan REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE Vulkan::Vulkan)
