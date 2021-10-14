#pragma once

#include "erase.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <string_view>

class VerifyDriveGeometry : public Erase
{
  public:
    VerifyDriveGeometry(std::string inDevPath) : Erase(inDevPath)
    {}
    /** @brief Creates a verifyDriveGeomentry erase object.
     *
     *  @param[in] inDevPath - the linux device path for the block device.
     */

    void geometryOkay(uint64_t bytes);
    /** @brief Test if input is in between the max and min expected sizes,
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     */
};
