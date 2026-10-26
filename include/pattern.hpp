#pragma once

#include "erase.hpp"
#include "util.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <chrono>
#include <cstdint>
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
     *  @param[in] inSeed - seed of the pseudo-random number generator. The
     *    same seed must be used to write and to verify the pattern.
     */
    Pattern(std::string_view inDevPath, uint64_t inSeed) :
        Erase(inDevPath), seed(inSeed)
    {}

    /** @brief writes an incompressible random pattern to the drive, using
     * default parameters. It also throws errors accordingly.
     */
    void writePattern()
    {
        const std::string& path = getDevPath();
        stdplus::fd::ManagedFd fd =
            stdplus::fd::open(path, stdplus::fd::OpenAccess::WriteOnly);
        writePattern(util::findSizeOfBlockDevice(path), fd);
        /* Make sure that the data reached the medium and report errors. */
        util::syncBlockDevice(fd.get(), path);
        /* Drop the written data from the page cache. */
        util::dropBlockDeviceCache(fd.get(), path);
    }

    /** @brief writes an incompressible random pattern to the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] fd - the stdplus file descriptor
     */
    void writePattern(uint64_t driveSize, Fd& fd);

    /** @brief verifies the incompressible random pattern is on the drive, using
     * default parameters. It also throws errors accordingly.
     */
    void verifyPattern()
    {
        const std::string& path = getDevPath();
        stdplus::fd::ManagedFd fd =
            stdplus::fd::open(path, stdplus::fd::OpenAccess::ReadOnly);
        /* Make sure that the data is read from the medium. */
        util::dropBlockDeviceCache(fd.get(), path);
        verifyPattern(util::findSizeOfBlockDevice(path), fd);
    }

    /** @brief verifies the incompressible random pattern is on the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] fd - the stdplus file descriptor
     */
    void verifyPattern(uint64_t driveSize, Fd& fd);

  private:
    /** @brief Seed of the pseudo-random number generator. */
    uint64_t seed;

    static constexpr size_t blockSize = 4096;
    static constexpr size_t blockSizeUsing64 = blockSize / sizeof(uint64_t);
    static constexpr size_t maxRetry = 32;
    static constexpr std::chrono::duration delay = std::chrono::milliseconds(1);
};

} // namespace estoraged
