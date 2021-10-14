#include "verifyDriveGeometry.hpp"

#include "estoraged_conf.hpp"
#include <xyz/openbmc_project/eStoraged/error.hpp>
#include <phosphor-logging/lg2.hpp>

#include <unistd.h>
#include <string>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

int VerifyDriveGeometry::geometryOkay(uint64_t bytes)
{
    if (bytes > ERASE_MAX_GEOMETRY)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("Erase verify Geometry to large",
            "REDFISH_MESSAGE_ID", msg,
            "REDFISH_MESSAGE_ARGS", bytes, ERASE_MAX_GEOMETRY);
        throw EraseError;
    }
    else if (bytes < ERASE_MIN_GEOMETRY)
    {
        std::string msg = "OpenBMC.0.1.DriveEraseFailure";
        lg2::error("eStorageD erase verify Geometry to small",
            "REDFISH_MESSAGE_ID", msg,
            "REDFISH_MESSAGE_ARGS", bytes, ERASE_MIN_GEOMETRY);
        throw EraseError;
    }
    else
    {
        std::string msg = "OpenBMC.0.1.DriveEraseSuccess";
        lg2::info("eStorageD erase verify Geometry in range",
            "REDFISH_MESSAGE_ID", msg);
    }
    return 0;
}
