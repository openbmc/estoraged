
#include "estoraged.hpp"

#include "logging.hpp"

#include <iostream>
#include <vector>

namespace estoraged
{

void eStoraged::format(std::vector<uint8_t>)
{
    std::cerr << "Formatting encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting Format"
                     "REDFISH_MESSAGE_ID=%s",
                     "eStorageD.1.0.StartFormat", NULL);
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting Erase"
                     "REDFISH_MESSAGE_ID=%s",
                     "eStorageD.1.0.StartErase", NULL);
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::cerr << "Locking encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting Lock"
                     "REDFISH_MESSAGE_ID=%s",
                     "eStorageD.1.0.StartLock", NULL);
}

void eStoraged::unlock(std::vector<uint8_t>)
{
    std::cerr << "Unlocking encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting unlock"
                     "REDFISH_MESSAGE_ID=%s",
                     "eStorageD.1.0.StartUnlock", NULL);
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting chaning password"
                     "REDFISH_MESSAGE_ID=%s",
                     "eStorageD.1.0.StartchangePassword", NULL);
}

} // namespace estoraged
