#include "util.hpp"

#include "estoraged_conf.hpp"
#include "getConfig.hpp"

#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

namespace estoraged
{
namespace util
{
using ::sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using ::stdplus::fd::ManagedFd;

uint64_t findSizeOfBlockDevice(const std::string& devPath)
{
    uint64_t bytes = 0;
    try
    {
        // open block dev
        ManagedFd fd =
            stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
        // get block size
        fd.ioctl(BLKGETSIZE64, &bytes);
    }
    catch (...)
    {
        lg2::error("erase unable to open blockdev", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS", devPath);
        throw InternalFailure();
    }
    return bytes;
}

void syncBlockDevice(int fd, std::string_view devPath)
{
    if (::fdatasync(fd) != 0)
    {
        const int err = errno;
        lg2::error("fdatasync() failed for {DEV}: {ERROR}", "DEV", devPath,
                   "ERROR", std::generic_category().message(err),
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.SyncBlockDeviceFailure"));
        throw InternalFailure();
    }
}

void dropBlockDeviceCache(int fd, std::string_view devPath)
{
    if (::ioctl(fd, BLKFLSBUF, 0) != 0)
    {
        const int err = errno;
        lg2::error("BLKFLSBUF failed for {DEV}: {ERROR}", "DEV", devPath,
                   "ERROR", std::generic_category().message(err),
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FlushBlockDeviceFailure"));
        throw InternalFailure();
    }

    /* posix_fadvise() returns an error number instead of setting errno. */
    const int err = ::posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    if (err != 0)
    {
        lg2::error("posix_fadvise() failed for {DEV}: {ERROR}", "DEV", devPath,
                   "ERROR", std::generic_category().message(err),
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FlushBlockDeviceFailure"));
        throw InternalFailure();
    }
}

ManagedFd openBlockDeviceDirect(const std::string& devPath,
                                stdplus::fd::OpenAccess access)
{
    try
    {
        return stdplus::fd::open(devPath, stdplus::fd::OpenFlags(access).set(
                                              stdplus::fd::OpenFlag::Direct));
    }
    catch (const std::system_error& e)
    {
        if (e.code() != std::errc::invalid_argument)
        {
            throw;
        }
        lg2::info("{DEV} does not support O_DIRECT; using buffered I/O", "DEV",
                  devPath);
    }
    return stdplus::fd::open(devPath, access);
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
    std::ifstream lifeTimeFile(sysfsPath + "/life_time");
    if (!(lifeTimeFile >> std::hex >> estA >> std::hex >> estB) ||
        (estA == 0) || (estA > 11) || (estB == 0) || (estB > 11))
    {
        lg2::error("Unable to read life_time from sysfs", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.LifeTimeFailure"));
        return 255;
    }

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

std::string getPartNumber(const std::filesystem::path& sysfsPath)
{
    std::ifstream partNameFile(sysfsPath / "name");
    std::string partName;
    if (!(partNameFile >> partName) || partName.empty())
    {
        lg2::error("Unable to read name from sysfs", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.PartNumberFailure"));
        return "unknown";
    }

    return partName;
}

std::string getSerialNumber(const std::filesystem::path& sysfsPath)
{
    std::ifstream serialNumberFile(sysfsPath / "serial");
    std::string serialNumber;
    if (!(serialNumberFile >> serialNumber) || serialNumber.empty())
    {
        lg2::error("Unable to read serial from sysfs", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.SerialNumberFailure"));
        return "unknown";
    }

    return serialNumber;
}

std::optional<DeviceInfo> findDevice(const StorageData& data,
                                     const std::filesystem::path& searchDir)
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
        return std::nullopt;
    }

    /* Check if location code/ silkscreen name is provided for the drive. */
    std::string locationCode;
    auto findLocationCode = data.find("LocationCode");
    if (findLocationCode != data.end())
    {
        const std::string* locationCodePtr =
            std::get_if<std::string>(&findLocationCode->second);
        if (locationCodePtr != nullptr)
        {
            locationCode = *locationCodePtr;
        }
    }

    /* Check if EraseMaxGeometry is provided. */
    uint64_t eraseMaxGeometry = ERASE_MAX_GEOMETRY;
    auto findEraseMaxGeometry = data.find("EraseMaxGeometry");
    if (findEraseMaxGeometry != data.end())
    {
        const auto* eraseMaxGeometryPtr =
            std::get_if<uint64_t>(&findEraseMaxGeometry->second);
        if (eraseMaxGeometryPtr != nullptr)
        {
            lg2::info("eStorageD new eraseMaxGeometry found on system");
            eraseMaxGeometry = *eraseMaxGeometryPtr;
        }
    }

    /* Check if EraseMinGeometry is provided. */
    uint64_t eraseMinGeometry = ERASE_MIN_GEOMETRY;
    auto findEraseMinGeometry = data.find("EraseMinGeometry");
    if (findEraseMinGeometry != data.end())
    {
        const auto* eraseMinGeometryPtr =
            std::get_if<uint64_t>(&findEraseMinGeometry->second);
        if (eraseMinGeometryPtr != nullptr)
        {
            lg2::info("eStorageD new eraseMinGeometry found on system");
            eraseMinGeometry = *eraseMinGeometryPtr;
        }
    }

    /*
     * Determine the drive type and protocol to report for this device. Note
     * that we only support eMMC currently, so report an error for any other
     * device types.
     */
    const std::string* deviceTypePtr = std::get_if<std::string>(&typeVariant);
    if (deviceTypePtr == nullptr)
    {
        lg2::error("Type property is not a string", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FindDeviceFail"));
        return std::nullopt;
    }
    const std::string& deviceType = *deviceTypePtr;
    /* drive type and protocol to report in the Item.Drive dbus interface */
    std::string driveType;
    std::string driveProtocol;
    if (deviceType.compare("EmmcDevice") == 0)
    {
        driveType = "SSD";
        driveProtocol = "eMMC";
    }
    else
    {
        lg2::error("Unsupported device type {TYPE}", "TYPE", deviceType,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FindDeviceFail"));
        return std::nullopt;
    }

    /* Look for the eMMC in the specified searchDir directory. */
    std::error_code ec;
    std::filesystem::directory_iterator dirIter(searchDir, ec);
    if (ec)
    {
        lg2::error("Failed to open search directory {DIR}: {ERROR}", "DIR",
                   searchDir, "ERROR", ec.message(), "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FindDeviceFail"));
        return std::nullopt;
    }

    for (const auto& dirEntry : dirIter)
    {
        /*
         * We will look at the 'type' file to determine if this is an MMC
         * device.
         */
        std::filesystem::path curPath(dirEntry.path());
        curPath /= "device/type";
        if (!std::filesystem::exists(curPath, ec) || ec)
        {
            /* The 'type' file doesn't exist. This must not be an eMMC. */
            continue;
        }

        try
        {
            std::ifstream typeFile(curPath, std::ios_base::in);
            std::string devType;
            typeFile >> devType;
            if (devType.compare("MMC") == 0 || devType.compare("SD") == 0)
            {
                /* Found it. Get the sysfs directory and device file. */
                std::filesystem::path deviceName(dirEntry.path().filename());

                std::filesystem::path sysfsDir = dirEntry.path();
                sysfsDir /= "device";

                std::filesystem::path deviceFile = "/dev";
                deviceFile /= deviceName;

                std::string luksName = "luks-" + deviceName.string();
                return DeviceInfo{
                    std::move(deviceFile), std::move(sysfsDir),
                    std::move(luksName),   std::move(locationCode),
                    eraseMaxGeometry,      eraseMinGeometry,
                    std::move(driveType),  std::move(driveProtocol)};
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
    return std::nullopt;
}

} // namespace util

} // namespace estoraged
