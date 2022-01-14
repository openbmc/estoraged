#pragma once

#include "cryptsetupInterface.hpp"
#include "erase.hpp"

#include <libcryptsetup.h>

#include <memory>
#include <string_view>
namespace estoraged
{
class CryptoErase : public Erase
{
  public:
    CryptoErase(std::string_view devPath, int keyslot,
                std::unique_ptr<estoraged::CryptsetupInterface>& cryptIface);
};

} // namespace estoraged
