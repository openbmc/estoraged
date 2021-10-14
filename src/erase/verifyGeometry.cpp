#include "verifyGeometry.hpp"

#include "estoraged_conf.hpp"
#include "logging.hpp"

#include <unistd.h>

int verifyGeometry::GeometryOkay(int (*findSize)(std::string, uint64_t& bytes))
{
    uint64_t bytes;
    if (findSize(m_devPath, bytes) != 0)
    {
        return false;
    }
    if (bytes >= ERASE_MAX_GEOMETRY)
    {
        estoragedLogging(
            "MESSAGE=%s", "Erase verify Geometry to large",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d >= %d", bytes, ERASE_MAX_GEOMETRY, NULL);
        return -1;
    }
    else if (bytes <= ERASE_MIN_GEOMETRY)
    {
        estoragedLogging(
            "MESSAGE=%s", "eStorageD erase verify Geometry to small",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseFailure",
            "REDFISH_MESSAGE_ARGS=%d <= %d", bytes, ERASE_MIN_GEOMETRY, NULL);
        return -1;
    }
    else
    {
        estoragedLogging(
            "MESSAGE=%s", "eStorageD erase verify Geometry in range",
            "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.DriveEraseSuccess", NULL);
    }
    return true;
}
