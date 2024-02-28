cmake_minimum_required(VERSION 3.13)
project(photon)
set(CMAKE_CXX_STANDARD 23)

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/cpm.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include (${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(${PROJECT_NAME} main.cpp ${SOURCES} ${HEADERS})

if(EMSCRIPTEN)
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
  target_link_options(${PROJECT_NAME} PRIVATE
          "-sUSE_WEBGPU=1"
          "-sUSE_GLFW=3"
          "-sALLOW_MEMORY_GROWTH=1"
          "--preload-file=${CMAKE_CURRENT_LIST_DIR}/res@res"
          "--shell-file=${CMAKE_CURRENT_LIST_DIR}/src/shell.html")
  target_link_libraries(${PROJECT_NAME} PRIVATE glm tinygltf)
else()
  set(DAWN_FETCH_DEPENDENCIES ON)
  add_subdirectory("lib/dawn" EXCLUDE_FROM_ALL)
  target_link_libraries(${PROJECT_NAME} PRIVATE webgpu_cpp webgpu_dawn webgpu_glfw glm tinygltf)
endif()

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 0.9.9.7
)

CPMAddPackage(
  NAME tiny_gltf
  GITHUB_REPOSITORY syoyo/tinygltf
  GIT_TAG v2.8.20
)