#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <string_view>

int findSizeOfBlockDevice(std::string_view devPath, uint64_t& bytes);

class VerifyDriveGeometry : erase
{
  public:
    VerifyDriveGeometry(std::string inDevPath) : erase(inDevPath)
    {}
    int geometryOkay(int (*func)(std::string_view, uint64_t&));
};
