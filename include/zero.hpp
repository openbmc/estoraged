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
    Zero(std::string_view inDevPath) : Erase(inDevPath), blockOfZeros{}
    {}

    /** @brief writes zero to the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] managedFd - the file descriptor for the open drive
     */
    void writeZero(uint64_t driveSize, ManagedFd& fd);

    /** @brief verifies the  uncompressible random pattern is on the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     *  @param[in] managedFd - the file descriptor for the open drive
     */
    void verifyZero(uint64_t driveSize, ManagedFd& fd);

  private:
    /* @brief the size of the blocks in bytes used for write and verify.
     * 32768 was also tested. It had almost identical performance.
     */
    static const size_t blockSize = 4096;

    /*@breif this is an emtpy block of (blockSize).
     * It is initized at construction and used in write and verify
     */
    const std::array<const std::byte, blockSize> blockOfZeros;
};

} // namespace estoraged
