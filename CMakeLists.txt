cmake_minimum_required(VERSION 3.13) # CMake version check
project(photon)                         # Create project "app"
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(${PROJECT_NAME} main.cpp ${SOURCES} ${HEADERS})

if(EMSCRIPTEN)
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
  target_link_options(${PROJECT_NAME} PRIVATE "-sUSE_WEBGPU=1" "-sUSE_GLFW=3")
else()
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("lib/dawn" EXCLUDE_FROM_ALL)
  target_link_libraries(${PROJECT_NAME} PRIVATE webgpu_cpp webgpu_dawn webgpu_glfw)
endif()
