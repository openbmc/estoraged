
#include "estoraged.hpp"

#include <iostream>
#include <vector>

namespace estoraged
{

void eStoraged::format(std::vector<uint8_t>)
{
  std::cerr << "Formatting encrypted eMMC" << std::endl;
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
  std::cerr << "Erasing encrypted eMMC" << std::endl;
}

void eStoraged::lock(std::vector<uint8_t>)
{
  std::cerr << "Locking encrypted eMMC" << std::endl;
}

void eStoraged::unlock(std::vector<uint8_t>)
{
  std::cerr << "Unlocking encrypted eMMC" << std::endl;
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
  std::cerr << "Changing password for encrypted eMMC" << std::endl;
}

} // namespace estoraged
