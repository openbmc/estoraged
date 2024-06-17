#include "cryptErase.hpp"
#include "cryptsetupInterface.hpp"
#include "erase.hpp"

#include <libcryptsetup.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace estoraged
{
using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;

CryptErase::CryptErase(
    std::string_view devPathIn,
    std::unique_ptr<estoraged::CryptsetupInterface> inCryptIface) :
    Erase(devPathIn),
    cryptIface(std::move(inCryptIface))
{}

void CryptErase::doErase()
{
    /* get cryptHandle */
    CryptHandle cryptHandle{devPath};
    /* cryptLoad */
    if (cryptIface->cryptLoad(cryptHandle.get(), CRYPT_LUKS2, nullptr) != 0)
    {
        lg2::error("Failed to load the key slots for destruction",
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }

    /* find key slots */
    int nKeySlots = cryptIface->cryptKeySlotMax(CRYPT_LUKS2);
    if (nKeySlots < 0)
    {
        lg2::error("Failed to find the max keyslots", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }

    if (nKeySlots == 0)
    {
        lg2::error("Max keyslots should never be zero", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }

    /* destroy working keyslots */
    bool keySlotIssue = false;
    for (int i = 0; i < nKeySlots; i++)
    {
        crypt_keyslot_info ki =
            cryptIface->cryptKeySlotStatus(cryptHandle.get(), i);

        if (ki == CRYPT_SLOT_ACTIVE || ki == CRYPT_SLOT_ACTIVE_LAST)
        {
            if (cryptIface->cryptKeyslotDestroy(cryptHandle.get(), i) != 0)
            {
                lg2::error(
                    "Estoraged erase failed to destroy keyslot, continuing",
                    "REDFISH_MESSAGE_ID",
                    std::string("eStorageD.1.0.EraseFailure"));
                keySlotIssue = true;
            }
        }
    }
    if (keySlotIssue)
    {
        throw InternalFailure();
    }
}

} // namespace estoraged
