#pragma once

#include "erase.hpp"

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
     *
     *  @param[in] bytes - Size of the block device
     */
    void writeZero(uint64_t driveSize);

    /** @brief verifies the  uncompressible random pattern is on the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     */
    void verifyZero(uint64_t driveSize);

  private:
    /* @brief the size of the blocks in bytes used for write and verify.
     * 32768 was also tested. It had almost identical performance.
     */
    static constexpr size_t blockSize = 4096;
};

} // namespace estoraged
