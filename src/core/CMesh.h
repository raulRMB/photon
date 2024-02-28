//
// Created by Raul Romero on 2024-02-27.
//

#ifndef PHOTON_CMESH_H
#define PHOTON_CMESH_H

#include "CComponent.h"
#include <webgpu/webgpu_cpp.h>
#include <vector>

namespace photon
{

struct CMesh : public CComponent
{
    const char* Path;
    i32 indexCount;
    wgpu::Buffer positionBuffer;
    std::vector<f32> pointData{};
    wgpu::Buffer normalBuffer;
    std::vector<f32> normalData{};
    wgpu::Buffer tangentBuffer;
    std::vector<f32> tangentData{};
    wgpu::Buffer bitangentBuffer;
    std::vector<f32> bitangentData{};
    wgpu::Buffer colorBuffer;
    std::vector<f32> colorData{};
    wgpu::Buffer uvBuffer;
    std::vector<f32> uvData{};

    wgpu::Buffer indexBuffer;
    std::vector<u16> indexData{};

};

} // photon

#endif //PHOTON_CMESH_H
