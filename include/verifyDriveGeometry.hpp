#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <string_view>

class VerifyDriveGeometry : public erase
{
  public:
    VerifyDriveGeometry(std::string inDevPath) : erase(inDevPath)
    {}
    int geometryOkay(uint64_t bytes);
};
