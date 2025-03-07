//
// Created by Raul Romero on 2024-02-23.
//

#ifndef PHOTON_TYPES_H
#define PHOTON_TYPES_H

#include <glm/glm.hpp>
#include <vector>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef float f32;
typedef double f64;

typedef glm::vec2 v2f;
typedef glm::vec3 v3f;
typedef glm::vec4 v4f;

typedef glm::ivec2 v2i;
typedef glm::ivec3 v3i;
typedef glm::ivec4 v4i;

typedef glm::mat2 m2;
typedef glm::mat3 m3;
typedef glm::mat4 m4;

typedef u8 byte;

template <typename T, u32 size>
using array = std::array<T, size>;
template <typename T>
using dArray = std::vector<T>;

#endif //PHOTON_TYPES_H
