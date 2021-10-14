
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
                     "OpenBMC.0.1.DriveFormat", NULL);
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting Erase"
                     "REDFISH_MESSAGE_ID=%s",
                     "OpenBMC.0.1.DriveErase", NULL);
  switch(inErase){
    case EraseMethod::VerifyGeometry:
      {
        verifyGeometry myVerifyGeometry(m_phyPath);
        myVerifyGeometry.GeometryOkay(findSizeOfBlockDevice);
        break;
      }
    default :
      std::cout << "enum not found" << std::endl;
   }
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::cerr << "Locking encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting Lock"
                     "REDFISH_MESSAGE_ID=%s",
                     "OpenBMC.0.1.DriveLock", NULL);
}

void eStoraged::unlock(std::vector<uint8_t>)
{
    std::cerr << "Unlocking encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting unlock"
                     "REDFISH_MESSAGE_ID=%s",
                     "OpenBMC.0.1.DriveUnlock", NULL);
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    estoragedLogging("MESSAGE=starting chaning password"
                     "REDFISH_MESSAGE_ID=%s",
                     "OpenBMC.0.1.DrivePasswordChanged", NULL);
}

} // namespace estoraged
