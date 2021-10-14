#include "logging.hpp"
#include "verifyGeometry.hpp"

#define BLKGETSIZE64                                                           \
    _IOR(0x12, 114, size_t) /* return device size in bytes (u64 *arg) */

extern int findSizeOfBlockDevice(std::string devPath, uint64_t& bytes)
{
    // blocking open
    int fd = open(devPath.c_str(), O_RDONLY, O_NDELAY);
    if (fd <= 0)
    {
        estoragedLogging("MESSAGE=%s", "erase unable to open blockdev",
                         "REDFISH_MESSAGE_ID=%s",
                         "OpenBMC.0.1.DriveEraseFailure",
                         "REDFISH_MESSAGE_ARGS=%d,%s", fd, devPath, NULL);
        return fd;
    }
    // get block size
    int rc = ioctl(fd, BLKGETSIZE64, &bytes);
    if (rc <= 0)
    {
        estoragedLogging("MESSAGE=%s", "erase unable to get size of device",
                         "REDFISH_MESSAGE_ID=%s",
                         "OpenBMC.0.1.DriveEraseFailure",
                         "REDFISH_MESSAGE_ARGS=%d,%s", rc, devPath, NULL);
    }
    return rc;
}
