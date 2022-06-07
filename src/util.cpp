#include "util.hpp"

#include "getConfig.hpp"

#include <linux/fs.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
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
    // in the extended CSD field 269 and 268, named estimate A and estimate B.
    // Linux exposes the values in the /life_time node.
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

bool findDevice(const StorageData& data, const std::filesystem::path& searchDir,
                std::filesystem::path& deviceFile,
                std::filesystem::path& sysfsDir, std::string& luksName)
{
    /* Check what type of storage device this is. */
    estoraged::BasicVariantType typeVariant;
    try
    {
        /* The EntityManager config should have this property set. */
        typeVariant = data.at("Type");
    }
    catch (const boost::container::out_of_range& e)
    {
        lg2::error("Could not read device type", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FindDeviceFail"));
        return false;
    }

    /*
     * Currently, we only support eMMC, so report an error for any other device
     * types.
     */
    std::string type = std::get<std::string>(typeVariant);
    if (type.compare("EmmcDevice") != 0)
    {
        lg2::error("Unsupported device type {TYPE}", "TYPE", type,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FindDeviceFail"));
        return false;
    }

    /* Look for the eMMC in the specified searchDir directory. */
    for (auto const& dirEntry : std::filesystem::directory_iterator{searchDir})
    {
        /*
         * We will look at the 'type' file to determine if this is an MMC
         * device.
         */
        std::filesystem::path curPath(dirEntry.path());
        curPath /= "device/type";
        if (!std::filesystem::exists(curPath))
        {
            /* The 'type' file doesn't exist. This must not be an eMMC. */
            continue;
        }

        try
        {
            std::ifstream typeFile(curPath, std::ios_base::in);
            std::string devType;
            typeFile >> devType;
            if (devType.compare("MMC") == 0)
            {
                /* Found it. Get the sysfs directory and device file. */
                std::filesystem::path deviceName(dirEntry.path().filename());

                sysfsDir = dirEntry.path();
                sysfsDir /= "device";

                deviceFile = "/dev";
                deviceFile /= deviceName;

                luksName = "luks-" + deviceName.string();
                return true;
            }
        }
        catch (...)
        {
            lg2::error("Failed to read device type for {PATH}", "PATH", curPath,
                       "REDFISH_MESSAGE_ID",
                       std::string("OpenBMC.0.1.FindDeviceFail"));
            /*
             * We will still continue searching, though. Maybe this wasn't the
             * device we were looking for, anyway.
             */
        }
    }

    /* Device wasn't found. */
    return false;
}

} // namespace util

} // namespace estoraged
