#pragma once

#include "erase.hpp"
#include "util.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

namespace estoraged
{

using stdplus::fd::ManagedFd;

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
     */
    void writeZero(uint64_t driveSize);

    /** @brief writes zero to the drive
     * and throws errors accordingly.
     */
    void writeZero()
    {
        writeZero(util::findSizeOfBlockDevice(devPath));
    }

    /** @brief verifies the  uncompressible random pattern is on the drive
     * and throws errors accordingly.
     *  @param[in] driveSize - the size of the block device in bytes
     */
    void verifyZero(uint64_t driveSize);

    /** @brief verifies the  uncompressible random pattern is on the drive
     * and throws errors accordingly.
     */
    void verifyZero()
    {
        verifyZero(util::findSizeOfBlockDevice(devPath));
    }

  private:
    /* @brief the size of the blocks in bytes used for write and verify.
     * 32768 was also tested. It had almost identical performance.
     */
    static constexpr size_t blockSize = 4096;
};

} // namespace estoraged
