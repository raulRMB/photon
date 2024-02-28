//
// Created by Raul Romero on 2024-02-27.
//

#ifndef PHOTON_CCAMERA_H
#define PHOTON_CCAMERA_H

#include "CComponent.h"

namespace photon
{

struct CCamera : public CComponent
{
    v3 Position;
    v3 Target;
    v3 Right;
    v3 Front;
    v3 Up;
    f32 Fov;
    f32 Aspect;
    f32 Near;
    f32 Far;
};

}// photon

#endif //PHOTON_CCAMERA_H
