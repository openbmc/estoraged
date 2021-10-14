#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <string_view>

class VerifyDriveGeometry : public Erase
{
  public:
    VerifyDriveGeometry(std::string inDevPath) : Erase(inDevPath)
    {}
    int geometryOkay(uint64_t bytes);
};
