#include "util.hpp"

#include <linux/fs.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <sstream>
#include <string_view>

namespace estoraged
{
namespace util
{
using ::sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using ::stdplus::fd::ManagedFd;

uint64_t Util::findSizeOfBlockDevice(const std::string& devPath)
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

static uint8_t findPredictedMediaLifeLeftPercent(const std::string& sysfsPath)
{
    ManagedFd fd;
    try
    {
        fd = stdplus::fd::open(sysfsPath + "/life_time",
                               stdplus::fd::OpenAccess::ReadOnly);
        std::array<std::byte, 7> readLifeTimeArr;
        fd.read(readLifeTimeArr);
        std::string readLifeTimeStr(std::begin(readLifeTimeArr),
                                    std::end(readLifeTimeArr));
        std::array<std::byte, 7> readLifeTimeChar;
        std::stringstream ss(readLifeTimeStr);
        uint8_t first, second;
        ss >> first;
        ss >> second;
        if (first > second)
        {
            return 100 - first;
        }
        return 100 - second;
    }
    catch (...)
    {
        lg2::error("Unable to read sysfs", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS", std::to_string(fd.get()));
        throw InternalFailure();
    }
    return 101;
}

} // namespace util

} // namespace estoraged
