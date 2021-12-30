#include "verifyDriveGeometry.hpp"

#include "estoraged_conf.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

void VerifyDriveGeometry::geometryOkay(uint64_t bytes)
{
    if (bytes > ERASE_MAX_GEOMETRY)
    {
        lg2::error("Erase verify Geometry too large", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DriveEraseFailure"),
                   "REDFISH_MESSAGE_ARGS",
                   std::to_string(bytes) + ">" +
                       std::to_string(ERASE_MAX_GEOMETRY));
        throw InternalFailure();
    }
    else if (bytes < ERASE_MIN_GEOMETRY)
    {
        lg2::error(
            "eStorageD erase verify Geometry too small", "REDFISH_MESSAGE_ID",
            std::string("OpenBMC.0.1.DriveEraseFailure"),
            "REDFISH_MESSAGE_ARGS",
            std::to_string(bytes) + "<" + std::to_string(ERASE_MIN_GEOMETRY));
        throw InternalFailure();
    }
    else
    {
        lg2::info("eStorageD erase verify Geometry in range",
                  "REDFISH_MESSAGE_ID",
                  std::string("OpenBMC.0.1.DriveEraseSuccess"));
    }
}
