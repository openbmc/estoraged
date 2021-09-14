
#include "estoraged.hpp"

#include <iostream>
#include <vector>

namespace openbmc
{

void eStoraged::format(std::vector<uint8_t> password)
{
  (void)password;
  std::cerr << "Formatting encrypted eMMC" << std::endl;
}

void eStoraged::erase(std::vector<uint8_t> password,
                      enum EraseMethod eraseType)
{
  (void)password;
  (void)eraseType;
  std::cerr << "Erasing encrypted eMMC" << std::endl;
}

void eStoraged::lock(std::vector<uint8_t> password)
{
  (void)password;
  std::cerr << "Locking encrypted eMMC" << std::endl;
}

void eStoraged::unlock(std::vector<uint8_t> password)
{
  (void)password;
  std::cerr << "Unlocking encrypted eMMC" << std::endl;
}

void eStoraged::changePassword(std::vector<uint8_t> oldPassword,
                               std::vector<uint8_t> newPassword)
{
  (void)oldPassword;
  (void)newPassword;
  std::cerr << "Changing password for encrypted eMMC" << std::endl;
}

} // namespace openbmc
