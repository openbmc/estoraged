#pragma once
#include <string_view>

namespace estoraged
{
namespace util
{

class Util
{
  public:
    /** @brief finds the size of the linux block device in bytes
     *  @param[in] devpath - the name of the linux block device
     *  @return size of a block device using the devPath
     */
    static uint64_t findSizeOfBlockDevice(const std::string& devPath);
};

} // namespace util

} // namespace estoraged
