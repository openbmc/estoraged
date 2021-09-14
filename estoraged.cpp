
#include "estoraged.hpp"

#include <iostream>
#include <vector>

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
