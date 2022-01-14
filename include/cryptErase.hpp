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
     */
    CryptErase(std::string_view devPath);

    /** @brief searches and deletes all cryptographic keyslot
     * and throws errors accordingly.
     *
     *  @param[in](optional) cryptIface - a unique pointer to an cryptsetup
     *  Interface object.
     */
    void doErase(std::unique_ptr<estoraged::CryptsetupInterface> cryptIface =
                     std::make_unique<Cryptsetup>());
};

} // namespace estoraged
