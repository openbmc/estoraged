#pragma once

#include "cryptsetupInterface.hpp"
#include "erase.hpp"

#include <libcryptsetup.h>

#include <memory>
#include <string_view>

namespace estoraged
{

class CryptErase : public Erase
{
  public:
    /** @brief Creates a CryptErase erase object.
     *
     *  @param[in] inDevPath - the linux device path for the block device.
     *  @param[in](optional) cryptIface - a unique pointer to an cryptsetup
     *  Interface object.
     */
    CryptErase(std::string_view devPath,
               std::unique_ptr<estoraged::CryptsetupInterface> inCryptIface =
                   std::make_unique<Cryptsetup>());

    /** @brief searches and deletes all cryptographic keyslot
     * and throws errors accordingly.
     */
    void doErase();

  private:
    std::unique_ptr<estoraged::CryptsetupInterface> cryptIface;
};

} // namespace estoraged
