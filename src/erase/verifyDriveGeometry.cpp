#include "verifyDriveGeometry.hpp"

#include "estoraged_conf.hpp"
#include "logging.hpp"

#include <unistd.h>

int VerifyDriveGeometry::geometryOkay(int (*findSize)(std::string_view,
                                                      uint64_t& bytes))
{
    uint64_t bytes;
    if (findSize(devPath, bytes) != 0)
    {
        return false;
    }
    if (bytes > ERASE_MAX_GEOMETRY)
    {
        log("MESSAGE=%s", "Erase verify Geometry to large",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d >= %d", bytes, ERASE_MAX_GEOMETRY, NULL);
        return -1;
    }
    else if (bytes < ERASE_MIN_GEOMETRY)
    {
        log("MESSAGE=%s", "eStorageD erase verify Geometry to small",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d <= %d", bytes, ERASE_MIN_GEOMETRY, NULL);
        return -1;
    }
    else
    {
        log("MESSAGE=%s", "eStorageD erase verify Geometry in range",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseSuccess", NULL);
    }
    return 0;
}
