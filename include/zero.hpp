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
    Zero(std::string_view inDevPath) : Erase(inDevPath)
    {}
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
        stdplus::fd::Fd&& fd =
            stdplus::fd::open(devPath, stdplus::fd::OpenAccess::WriteOnly);
        writeZero(util::findSizeOfBlockDevice(devPath), fd);
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
        stdplus::fd::Fd&& fd =
            stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
        verifyZero(util::findSizeOfBlockDevice(devPath), fd);
    }

  private:
    /* @brief the size of the blocks in bytes used for write and verify.
     * 32768 was also tested. It had almost identical performance.
     */
    static constexpr size_t blockSize = 4096;
    static constexpr size_t maxRetry = 32;
    static constexpr std::chrono::duration delay = std::chrono::milliseconds(1);
};

} // namespace estoraged
