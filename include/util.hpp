#pragma once
#include <string_view>

namespace estoraged
{
namespace util
{

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

} // namespace util

} // namespace estoraged
