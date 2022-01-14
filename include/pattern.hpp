#pragma once

#include "erase.hpp"

#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>

#include <span>
#include <string>

namespace estoraged
{
using stdplus::fd::ManagedFd;

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
     */
    void writePattern(uint64_t driveSize);

    /** @brief verifies the uncompressible random pattern is on the drive
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     */
    void verifyPattern(uint64_t driveSize);
};

} // namespace estoraged
