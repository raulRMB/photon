//
// Created by Raul Romero on 2024-02-23.
//

#ifndef PHOTON_PHOTONCORE_H
#define PHOTON_PHOTONCORE_H

#include "Types.h"
#include <iostream>

#define PHOTON_VERSION_MAJOR 0
#define PHOTON_VERSION_MINOR 1
#define PHOTON_VERSION_PATCH 0

#define PHOTON_VERSION_STRING "0.1.0"

#define PHOTON_VERSION PHOTON_VERSION_MAJOR, PHOTON_VERSION_MINOR, PHOTON_VERSION_PATCH

#define PHOTON_LOG(msg) std::cout << msg << std::endl
#define PHOTON_LOG_ERROR(msg) std::cerr << msg << std::endl

#endif //PHOTON_PHOTONCORE_H
