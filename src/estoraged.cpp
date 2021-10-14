
#include "estoraged.hpp"

#include "verifyDriveGeometry.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <iostream>
#include <vector>

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EraseError;

namespace estoraged
{

void eStoraged::format(std::vector<uint8_t>)
{
    std::cerr << "Formatting encrypted eMMC" << std::endl;
    std::string msg = "OpenBMC.0.1.DriveFormat";
    lg2::info("Starting format", "REDFISH_MESSAGE_ID", msg);
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod inEraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    lg2::info("Starting erase", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveErase"));
    switch (inEraseMethod)
    {
        case EraseMethod::VerifyGeometry:
        {
            VerifyDriveGeometry myVerifyGeometry(devPath);
            uint64_t size = myVerifyGeometry.findSizeOfBlockDevice();
            myVerifyGeometry.geometryOkay(size);
            break;
        }
        default:
            throw EraseError();
    }
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::cerr << "Locking encrypted eMMC" << std::endl;
    lg2::info("Starting lock", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveLock"));
}

void eStoraged::unlock(std::vector<uint8_t>)
{
    std::cerr << "Unlocking encrypted eMMC" << std::endl;
    lg2::info("Starting unlock", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DriveUnlock"));
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    lg2::info("Starting change password", "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DrivePasswordChanged"));
}

} // namespace estoraged
