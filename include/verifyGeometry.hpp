#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <systemd/sd-journal.h>

int findSizeOfBlockDevice(std::string devPath, uint64_t& bytes);

class verifyGeometry : erase
{
  public:
    verifyGeometry(std::string inDevPath) : erase(inDevPath)
    {}
    int GeometryOkay(int (*func)(std::string, uint64_t&));
};
