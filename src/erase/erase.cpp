#include "erase.hpp"

#include <linux/fs.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;
using stdplus::fd::ManagedFd;

uint64_t Erase::findSizeOfBlockDevice()
{
    // blocking open
    ManagedFd fd;
    uint64_t bytes;
    try
    {
        fd = stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
        // get block size
        fd.ioctl(BLKGETSIZE64, &bytes);
    }
    catch (...)
    {
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw EraseError();
    }
    return bytes;
}
