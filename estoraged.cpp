
#include "estoraged.hpp"

#include <iostream>
#include <vector>

namespace openbmc
{

void eStoraged::format(std::vector<uint8_t> encryptionPassword)
{
  std::cerr << "functionality not implemented: Formatting encrypted eMMC" << std::endl;
}

void eStoraged::erase(std::vector<uint8_t> encryptionPassword,
                      std::vector<uint8_t> devicePassword,
                      bool fullDiskErase)
{
  std::cerr << "functionality not implemented: Erasing encrypted eMMC" << std::endl;
}

void eStoraged::lock(std::vector<uint8_t> EncryptionPassword)
{
  std::cerr << "functionality not implemented: Locking encrypted eMMC" << std::endl;
}

void eStoraged::unlock(std::vector<uint8_t> encryptionPassword,
                       std::vector<uint8_t> devicePassword)
{
  std::cerr << "functionality not implemented: Unlocking encrypted eMMC" << std::endl;
}

void eStoraged::changePassword(std::vector<uint8_t> oldEncryptionPassword,
                               std::vector<uint8_t> oldDevicePassword,
                               std::vector<uint8_t> newEncryptionPassword,
                               std::vector<uint8_t> newDevicePassword)
{
  std::cerr << "functionality not implemented: Changing password for encrypted eMMC" << std::endl;
}

} // namespace openbmc
