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
    static m3 CalculateTangent(const v3& p1, const v3& p2, const v3& p3, const v2& uv1, const v2& uv2, const v2& uv3);
};

} // photon

#endif //PHOTON_RESOURCELOADER_H
