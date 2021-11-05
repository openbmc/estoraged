
#include "estoraged.hpp"

#include "erase/verifyGeometry.hpp"
#include "erase/sanitize.hpp"
#include <iostream>
#include <vector>
#include <systemd/sd-journal.h>
#include <chrono>

namespace openbmc
{

void eStoraged::format(std::vector<uint8_t> encryptionPassword,
                       std::vector<uint8_t> devicePassword)
{
  std::cout << "Formatting encrypted eMMC" << std::endl;
}

void eStoraged::erase(std::vector<uint8_t> encryptionPassword,
                      std::vector<uint8_t> devicePassword,
                      EraseMethod inErase)
{
  sd_journal_send("MESSAGE=starting Erase"
                  "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.StartErase",
                   NULL);
  switch(inErase){
    case EraseMethod::VerifyGeometry:
      {
        verifyGeometry myVerifyGeometry(m_phyPath);
        myVerifyGeometry.GeometryOkay(findSizeOfBlockDevice);
        break;
      }
     case EraseMethod::VendorSanitizie:
      {
        std::chrono::minutes delay(20);
        sanitize mySanitize(m_phyPath, delay, ioctl);
        break;
      }
    default :
      std::cout << "enum not found" << std::endl;
   }

  std::cout << "Erasing encrypted eMMC" << std::endl;

}

void eStoraged::lock(std::vector<uint8_t> devicePassword)
{
  std::cout << "Locking encrypted eMMC" << std::endl;
}

void eStoraged::unlock(std::vector<uint8_t> encryptionPassword,
                       std::vector<uint8_t> devicePassword)
{
  std::cout << "Unlocking encrypted eMMC" << std::endl;
}

void eStoraged::changePassword(std::vector<uint8_t> oldEncryptionPassword,
                               std::vector<uint8_t> oldDevicePassword,
                               std::vector<uint8_t> newEncryptionPassword,
                               std::vector<uint8_t> newDevicePassword)
{
  std::cout << "Changing password for encrypted eMMC" << std::endl;
}

} // namespace openbmc
