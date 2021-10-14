#pragma once

#include "erase.hpp"
#include <systemd/sd-journal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>


int findSizeOfBlockDevice(std::string devPath, unsigned long &bytes);

class verifyGeometry : erase{
  public:
    verifyGeometry(std::string inDevPath): erase(inDevPath)
      {}
    int GeometryOkay( int(*func)(std::string, unsigned long& ));
};

