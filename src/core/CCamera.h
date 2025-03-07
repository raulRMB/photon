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
    v3f Position;
    v3f Target;
    v3f Right;
    v3f Front;
    v3f Up;
    f32 Fov;
    f32 Aspect;
    f32 Near;
    f32 Far;
};

}// photon

#endif //PHOTON_CCAMERA_H
