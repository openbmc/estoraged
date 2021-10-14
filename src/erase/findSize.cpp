#include "logging.hpp"
#include "verifyDriveGeometry.hpp"

#define BLKGETSIZE64                                                           \
    _IOR(0x12, 114, size_t) /* return device size in bytes (u64 *arg) */

int findSizeOfBlockDevice(std::string_view devPath, uint64_t& bytes)
{
    std::string tmpStr = std::string(devPath);
    // blocking open
    int fd = open(tmpStr.c_str(), O_RDONLY, O_NDELAY);
    if (fd <= 0)
    {
        log("MESSAGE=%s", "erase unable to open blockdev",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d,%s", fd, devPath, NULL);
        return fd;
    }
    // get block size
    int rc = ioctl(fd, BLKGETSIZE64, &bytes);
    if (rc <= 0)
    {
        log("MESSAGE=%s", "erase unable to get size of device",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d,%s", rc, devPath, NULL);
    }
    return rc;
}
