
#include "estoraged.hpp"

#include <iostream>
#include <vector>
#include <systemd/sd-journal.h>

namespace openbmc
{

void eStoraged::format(std::vector<uint8_t> encryptionPassword,
                       std::vector<uint8_t> devicePassword)
{
  std::cout << "Formatting encrypted eMMC" << std::endl;
}

void eStoraged::erase(std::vector<uint8_t> encryptionPassword,
                      std::vector<uint8_t> devicePassword,
                      bool fullDiskErase)
{
  sd_journal_send("MESSAGE=starting Erase"
                  "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.StartErase",
                   NULL);

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
