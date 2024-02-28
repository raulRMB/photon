//
// Created by Raul Romero on 2024-02-27.
//

#ifndef PHOTON_LOGGER_H
#define PHOTON_LOGGER_H

#include "PhotonCore.h"

namespace photon
{

enum class LogLevel
{
    Info = 0,
    Warning,
    Error
};

void Log(LogLevel logLevel, const char* format, ...);
void LogInfo(const char* format, ...);
void LogWarning(const char* format, ...);
void LogError(const char* format, ...);

} // photon

#endif //PHOTON_LOGGER_H
