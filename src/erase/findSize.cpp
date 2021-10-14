#include "verifyDriveGeometry.hpp"

#include <phosphor-logging/lg2.hpp>
#include <linux/fs.h>

#include <xyz/openbmc_project/eStoraged/error.hpp>
using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;


uint64_t findSizeOfBlockDevice(std::string_view devPath)
{
    std::string tmpStr = std::string(devPath);
    // blocking open
    int fd = open(tmpStr.c_str(), O_RDONLY, O_NDELAY);
    if (fd <= 0)
    {
      std::string msg = "OpenBMC.0.1.DriveEraseFailure";
      lg2::error("erase unable to open blockdev",
            "REDFISH_MESSAGE_ID", msg,
            "REDFISH_MESSAGE_ARGS", fd, devPath);
        throw EraseError;
    }
    // get block size
    uint64_t bytes;
    int rc = ioctl(fd, BLKGETSIZE64, &bytes);
    if (rc <= 0)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("erase unable to get size of device",
            "REDFISH_MESSAGE_ID", msg,
            "REDFISH_MESSAGE_ARGS", rc, devPath);
        throw EraseError;
    }
    return bytes;
}
