
#include "estoraged.hpp"

#include "logging.hpp"

#include <iostream>
#include <vector>

namespace estoraged
{

void eStoraged::format(std::vector<uint8_t>)
{
    std::cerr << "Formatting encrypted eMMC" << std::endl;
    log("MESSAGE=Starting format"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveFormat", NULL);
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    log("MESSAGE=Starting erase"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveErase", NULL);
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::cerr << "Locking encrypted eMMC" << std::endl;
    log("MESSAGE=Starting lock"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveLock", NULL);
}

void eStoraged::unlock(std::vector<uint8_t>)
{
    std::cerr << "Unlocking encrypted eMMC" << std::endl;
    log("MESSAGE=Starting unlock"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveUnlock", NULL);
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    log("MESSAGE=Starting change password"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DrivePasswordChanged", NULL);
}

} // namespace estoraged
