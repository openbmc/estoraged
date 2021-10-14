#include "erase.hpp"

#include <linux/fs.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

uint64_t erase::findSizeOfBlockDevice()
{
    std::string tmpStr = std::string(devPath);
    // blocking open
    ManagedFd fd(open(tmpStr.c_str(), O_RDONLY, O_NDELAY));
    if (fd.get() <= 0)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID", msg,
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()) + tmpStr);
        throw EraseError();
    }
    // get block size
    uint64_t bytes;
    int rc = fd.ioctl(BLKGETSIZE64, &bytes);
    if (rc <= 0)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("erase unable to get size of device", "REDFISH_MESSAGE_ID",
                   msg, "REDFISH_MESSAGE_ARGS", std::to_string(rc) + tmpStr);
        throw EraseError();
    }
    return bytes;
}
