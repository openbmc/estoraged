#pragma once

#include "erase.hpp"
#include "util.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <span>
#include <string>

namespace estoraged
{
using stdplus::fd::Fd;

class Pattern : public Erase
{
  public:
    /** @brief Creates a pattern erase object.
     *
     *  @param[in] inDevPath - the linux device path for the block device.
     */
    Pattern(std::string_view inDevPath) : Erase(inDevPath)
    {}

    /** @brief writes an uncompressible random pattern to the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] fd - the stdplus file descriptor
     */
    void writePattern()
    {
        stdplus::fd::Fd&& fd =
            stdplus::fd::open(devPath, stdplus::fd::OpenAccess::WriteOnly);
        writePattern(util::Util::findSizeOfBlockDevice(devPath), fd);
    }

    void writePattern(uint64_t driveSize, Fd& fd);

    /** @brief verifies the uncompressible random pattern is on the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] fd - the stdplus file descriptor
     */

    void verifyPattern()
    {
        stdplus::fd::Fd&& fd =
            stdplus::fd::open(devPath, stdplus::fd::OpenAccess::ReadOnly);
        verifyPattern(util::Util::findSizeOfBlockDevice(devPath), fd);
    }
    void verifyPattern(uint64_t driveSize, Fd& fd);

  private:
    static constexpr uint32_t seed = 0x6a656272;
    static constexpr size_t blockSize = 4096;
    static constexpr size_t blockSizeUsing32 = blockSize / sizeof(uint32_t);
    static constexpr uint16_t maxRetry = 32;
    static constexpr uint16_t delay = 1000;
};

} // namespace estoraged
