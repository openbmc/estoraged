#include "verifyDriveGeometry.hpp"

#include "estoraged_conf.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <string>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

void VerifyDriveGeometry::geometryOkay(uint64_t bytes)
{
    if (bytes > ERASE_MAX_GEOMETRY)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("Erase verify Geometry too large", "REDFISH_MESSAGE_ID", msg,
                   "REDFISH_MESSAGE_ARGS",
                   std::to_string(bytes) + ">" +
                       std::to_string(ERASE_MAX_GEOMETRY));
        throw EraseError();
    }
    else if (bytes < ERASE_MIN_GEOMETRY)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("eStorageD erase verify Geometry too small",
                   "REDFISH_MESSAGE_ID", msg, "REDFISH_MESSAGE_ARGS",
                   std::to_string(bytes) + "<" +
                       std::to_string(ERASE_MIN_GEOMETRY));
        throw EraseError();
    }
    else
    {
        std::string msg = "OpenBMC.0.1.DriveEraseSuccess";
        lg2::info("eStorageD erase verify Geometry in range",
                  "REDFISH_MESSAGE_ID", msg);
    }
}
