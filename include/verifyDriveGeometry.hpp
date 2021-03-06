#pragma once

#include "erase.hpp"
#include "util.hpp"

#include <string_view>

namespace estoraged
{

class VerifyDriveGeometry : public Erase
{
  public:
    /** @brief Creates a verifyDriveGeomentry erase object.
     *
     *  @param[in] inDevPath - the linux device path for the block device.
     */
    VerifyDriveGeometry(std::string_view inDevPath) : Erase(inDevPath)
    {}

    /** @brief Test if input is in between the max and min expected sizes,
     * and throws errors accordingly.
     *
     *  @param[in] bytes - Size of the block device
     */
    void geometryOkay()
    {
        geometryOkay(util::findSizeOfBlockDevice(devPath));
    }
    void geometryOkay(uint64_t bytes);
};

} // namespace estoraged
