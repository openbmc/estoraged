#pragma once
#include "getConfig.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace estoraged
{
namespace util
{

struct DeviceInfo
{
    std::filesystem::path deviceFile;
    std::filesystem::path sysfsDir;
    std::string luksName;
    std::string locationCode;
    uint64_t eraseMaxGeometry;
    uint64_t eraseMinGeometry;

    DeviceInfo(std::filesystem::path& deviceFile,
               std::filesystem::path& sysfsDir, std::string& luksName,
               std::string& locationCode, uint64_t eraseMaxGeometry,
               uint64_t eraseMinGeometry) :
        deviceFile(deviceFile),
        sysfsDir(sysfsDir), luksName(luksName), locationCode(locationCode),
        eraseMaxGeometry(eraseMaxGeometry), eraseMinGeometry(eraseMinGeometry)
    {}
};

/** @brief finds the size of the linux block device in bytes
 *  @param[in] devpath - the name of the linux block device
 *  @return size of a block device using the devPath
 */
uint64_t findSizeOfBlockDevice(const std::string& devPath);

/** @brief finds the predicted life left for a eMMC device
 *  @param[in] sysfsPath - The path to the linux sysfs interface
 *  @return the life remaing for the emmc, as a percentage.
 */
uint8_t findPredictedMediaLifeLeftPercent(const std::string& sysfsPath);

/** @brief Get the part number (aka part name) for the storage device
 *  @param[in] sysfsPath - The path to the linux sysfs interface.
 *  @return part name as a string (or "unknown" if it couldn't be retrieved)
 */
std::string getPartNumber(const std::filesystem::path& sysfsPath);

/** @brief Get the serial number for the storage device
 *  @param[in] sysfsPath - The path to the linux sysfs interface.
 *  @return serial name as a string (or "unknown" if it couldn't be retrieved)
 */
std::string getSerialNumber(const std::filesystem::path& sysfsPath);

/** @brief Look for the device described by the provided StorageData.
 *  @details Currently, this function assumes that there's only one eMMC.
 *    When we need to support multiple eMMCs, we will put more information in
 *    the EntityManager config, to differentiate between them. Also, if we
 *    want to support other types of storage devices, this function will need
 *    to be updated.
 *
 *  @param[in] data - map of properties from the config object.
 *  @param[in] searchDir - directory to search for devices in sysfs, e.g.
 *    /sys/block
 *  @param[out] DeviceInfo - metadata for the device if device is found. Null
 *  otherwise.
 */
std::optional<DeviceInfo> findDevice(const StorageData& data,
                                     const std::filesystem::path& searchDir);

} // namespace util

} // namespace estoraged
