#pragma once

#include "erase.hpp"
#include "util.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <chrono>

namespace estoraged
{

using stdplus::fd::Fd;

class Zero : public Erase
{
  public:
    /** @brief Creates a zero erase object.
     *
     *  @param[in] inDevPath - the linux device path for the block device.
     */
    Zero(std::string_view inDevPath) : Erase(inDevPath) {}
    /** @brief writes zero to the drive
     * and throws errors accordingly.
     *  @param[in] driveSize - the size of the block device in bytes
     *  @param[in] fd - the stdplus file descriptor
     */
    void writeZero(uint64_t driveSize, Fd& fd);

    /** @brief writes zero to the drive using default parameters,
     * and throws errors accordingly.
     */
    void writeZero()
    {
        const std::string& path = getDevPath();
        stdplus::fd::ManagedFd fd = util::openBlockDeviceDirect(
            path, stdplus::fd::OpenAccess::WriteOnly);
        writeZero(util::findSizeOfBlockDevice(path), fd);
        /* Make sure that the data reached the medium and report errors. */
        util::syncBlockDevice(fd.get(), path);
        /* Drop the written data from the page cache. */
        util::dropBlockDeviceCache(fd.get(), path);
    }

    /** @brief verifies the drive has only zeros on it,
     * and throws errors accordingly.
     *  @param[in] driveSize - the size of the block device in bytes
     *  @param[in] fd - the stdplus file descriptor
     */
    void verifyZero(uint64_t driveSize, Fd& fd);

    /** @brief verifies the drive has only zeros on it,
     * using the default parameters. It also throws errors accordingly.
     */
    void verifyZero()
    {
        const std::string& path = getDevPath();
        stdplus::fd::ManagedFd fd = util::openBlockDeviceDirect(
            path, stdplus::fd::OpenAccess::ReadOnly);
        /* Make sure that the data is read from the medium. */
        util::dropBlockDeviceCache(fd.get(), path);
        verifyZero(util::findSizeOfBlockDevice(path), fd);
    }

  private:
    /** @brief Number of bytes per read() or write() call. Large I/O requests
     *  minimize the number of system calls. A multiple of the logical block
     *  size as required for O_DIRECT.
     */
    static constexpr size_t blockSize = 1024 * 1024;
    static constexpr size_t maxRetry = 32;
    static constexpr std::chrono::duration delay = std::chrono::milliseconds(1);
};

} // namespace estoraged
