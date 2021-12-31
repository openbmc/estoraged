#pragma once

#include "erase.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

using stdplus::fd::ManagedFd;

constexpr size_t blockSize = 32768;

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
    /* @ this block of zeros is used in both writing and reading
     * zeros to the device
     */
    // static const std::array<std::byte, blockSize> blockOfZeros;
    const std::array<const std::byte, blockSize> blockOfZeros;
};
