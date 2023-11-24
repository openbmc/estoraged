#include "verifyDriveGeometry.hpp"

#include "util.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>

namespace estoraged
{
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

void VerifyDriveGeometry::geometryOkay(uint64_t bytes)
{
    const auto eraseMaxGeometry = util::getEraseMaxGeometry();
    if (bytes > eraseMaxGeometry)
    {
        lg2::error("Erase verify Geometry too large", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS",
                   std::to_string(bytes) + ">" +
                       std::to_string(eraseMaxGeometry));
        throw InternalFailure();
    }
    const auto eraseMinGeometry = util::getEraseMinGeometry();
    if (bytes < eraseMinGeometry)
    {
        lg2::error(
            "eStorageD erase verify Geometry too small", "REDFISH_MESSAGE_ID",
            std::string("OpenBMC.0.1.DriveEraseFailure"),
            "REDFISH_MESSAGE_ARGS",
            std::to_string(bytes) + "<" + std::to_string(eraseMinGeometry));
        throw InternalFailure();
    }

    lg2::info("eStorageD erase verify Geometry in range", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveEraseSuccess"));
}

} // namespace estoraged
