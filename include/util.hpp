#pragma once
#include "getConfig.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

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
    std::string driveType;
    std::string driveProtocol;

    DeviceInfo(std::filesystem::path deviceFile, std::filesystem::path sysfsDir,
               std::string luksName, std::string locationCode,
               uint64_t eraseMaxGeometry, uint64_t eraseMinGeometry,
               std::string driveType, std::string driveProtocol) :
        deviceFile(std::move(deviceFile)), sysfsDir(std::move(sysfsDir)),
        luksName(std::move(luksName)), locationCode(std::move(locationCode)),
        eraseMaxGeometry(eraseMaxGeometry), eraseMinGeometry(eraseMinGeometry),
        driveType(std::move(driveType)), driveProtocol(std::move(driveProtocol))
    {}
};

/** @brief Heap-allocated array whose start is aligned to a page boundary,
 *  as required for O_DIRECT I/O.
 */
template <typename T>
class AlignedBuffer
{
  public:
    static constexpr size_t alignment = 4096;

    /** @brief Allocates a zero-initialized buffer for @p count elements.
     *  @param[in] count - number of elements. count * sizeof(T) must be a
     *    multiple of the alignment.
     */
    explicit AlignedBuffer(size_t count) :
        data(static_cast<T*>(std::aligned_alloc(alignment, count * sizeof(T)))),
        count(count)
    {
        if (!data)
        {
            throw std::bad_alloc();
        }
        std::uninitialized_value_construct_n(data.get(), count);
    }

    std::span<T> span()
    {
        return {data.get(), count};
    }

    std::span<const T> span() const
    {
        return {data.get(), count};
    }

  private:
    struct Free
    {
        void operator()(T* p) const
        {
            std::free(p);
        }
    };

    std::unique_ptr<T, Free> data;
    size_t count;
};

/** @brief Opens a block device for direct I/O, i.e. such that reads and
 *  writes bypass the page cache. Falls back to buffered I/O if the device or
 *  file system does not support O_DIRECT.
 *  @param[in] devPath - path of the block device
 *  @param[in] access - access mode
 *  @return the file descriptor
 */
stdplus::fd::ManagedFd openBlockDeviceDirect(const std::string& devPath,
                                             stdplus::fd::OpenAccess access);

/** @brief finds the size of the linux block device in bytes
 *  @param[in] devpath - the name of the linux block device
 *  @return size of a block device using the devPath
 */
uint64_t findSizeOfBlockDevice(const std::string& devPath);

/** @brief Write the data written to a block device to the storage medium
 *  @details Calls fdatasync() such that all data written through the file
 *    descriptor has reached the storage medium when this function returns
 *    and such that writeback errors are reported.
 *  @param[in] fd - file descriptor of the block device
 *  @param[in] devPath - path of the block device, used for logging
 *  @throw InternalFailure if fdatasync() fails
 */
void syncBlockDevice(int fd, std::string_view devPath);

/** @brief Write back dirty data and drop the cached data of a block device
 *  @details Calls ioctl(BLKFLSBUF) and posix_fadvise(POSIX_FADV_DONTNEED)
 *    such that subsequent reads are served from the storage medium instead
 *    of from the page cache.
 *  @param[in] fd - file descriptor of the block device
 *  @param[in] devPath - path of the block device, used for logging
 *  @throw InternalFailure if either call fails
 */
void dropBlockDeviceCache(int fd, std::string_view devPath);

/** @brief finds the predicted life left for a eMMC device
 *  @param[in] sysfsPath - The path to the linux sysfs interface
 *  @return the life remaining for the emmc, as a percentage.
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
 *  @return DeviceInfo - metadata for the device if device is found. Null
 *  otherwise.
 */
std::optional<DeviceInfo> findDevice(const StorageData& data,
                                     const std::filesystem::path& searchDir);

} // namespace util

} // namespace estoraged
