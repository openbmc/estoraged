#include "util.hpp"

#include <linux/fs.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace estoraged
{
namespace util
{
using ::sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using ::stdplus::fd::ManagedFd;

uint64_t findSizeOfBlockDevice(const std::string& devPath)
{
    ManagedFd fd;
    uint64_t bytes = 0;
    try
    {
        // open block dev
        fd = stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
        // get block size
        fd.ioctl(BLKGETSIZE64, &bytes);
    }
    catch (...)
    {
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw InternalFailure();
    }
    return bytes;
}

uint8_t findPredictedMediaLifeLeftPercent(const std::string& sysfsPath)
{
    // The eMMC spec defines two estimates for the life span of the device
    // in the extended CSD field 269 and 268, named estimate A and estimate B
    // Linux eposes the values in the /life_time node.
    // estimate A is for A type memory
    // estimate B is for B type memory
    //
    // the estimate are encoded as such
    // 0x01 <=>  0% - 10% device life time used
    // 0x02 <=>  10% -20% device life time used
    // ...
    // 0x0A <=>  90% - 100% device life time used
    // 0x0B <=> Exceeded its maximum estimated device life time

    uint16_t estA = 0, estB = 0;
    std::ifstream lifeTimeFile;
    try
    {
        lifeTimeFile.open(sysfsPath + "/life_time", std::ios_base::in);
        lifeTimeFile >> std::hex >> estA;
        lifeTimeFile >> std::hex >> estB;
        if ((estA == 0) || (estA > 11) || (estB == 0) || (estB > 11))
        {
            throw InternalFailure();
        }
    }
    catch (...)
    {
        lg2::error("Unable to read sysfs", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"));
        lifeTimeFile.close();
        return 255;
    }
    lifeTimeFile.close();
    // we are returning lowest LifeLeftPercent
    uint8_t maxLifeUsed = 0;
    if (estA > estB)
    {
        maxLifeUsed = estA;
    }
    else
    {
        maxLifeUsed = estB;
    }

    return static_cast<uint8_t>(11 - maxLifeUsed) * 10;
}

} // namespace util

} // namespace estoraged
