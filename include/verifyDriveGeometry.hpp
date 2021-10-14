#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <string_view>

uint64_t findSizeOfBlockDevice(std::string_view devPath);

class VerifyDriveGeometry : erase
{
  public:
    VerifyDriveGeometry(std::string inDevPath) : erase(inDevPath)
    {}
    int geometryOkay(uint64_t bytes);
};
