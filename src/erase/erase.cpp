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
    ManagedFd fd = stdplus::fd::open(std::string(devPath).c_str(),
                                     stdplus::fd::OpenAccess::ReadOnly);
    if (fd.get() <= 0)
    {
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID",
                   std::string("openbmc.0.1.driveerasefailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw EraseError();
    }
    // get block size
    uint64_t bytes;
    fd.ioctl(BLKGETSIZE64, &bytes);
    return bytes;
}
