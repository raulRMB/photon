//
// Created by Raul Romero on 2024-02-27.
//

#ifndef PHOTON_RESOURCELOADER_H
#define PHOTON_RESOURCELOADER_H

#include "PhotonCore.h"
#include "CMesh.h"
#include <webgpu/webgpu_cpp.h>

namespace photon
{

enum class EModelImportType
{
    glb,
    gltf,
    obj,
    fbx,
    max,
    blender,
    unknown
};

enum class ETextureImportType
{
    png,
    jpg,
    tga,
    dds,
    ktx,
    unknown
};

class ResourceLoader
{
public:
    static CMesh LoadMesh(const char* path, const wgpu::Device& device, EModelImportType modelType = EModelImportType::glb);
    static wgpu::Texture LoadTexture(const char* path, wgpu::Device& device, ETextureImportType importType,
                                     wgpu::TextureView* pTextureView = nullptr);
    static wgpu::Texture LoadCubeMap(const char* path, wgpu::Device& device, ETextureImportType importType,
                                     wgpu::TextureView* pTextureView = nullptr);

    static u32 BitWidth(u32 m);
private:
    static m3 CalculateTangent(const v3f& p1, const v3f& p2, const v3f& p3, const v2f& uv1, const v2f& uv2, const v2f& uv3);
};

} // photon

#endif //PHOTON_RESOURCELOADER_H
