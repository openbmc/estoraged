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
    std::unique_ptr<estoraged::CryptsetupInterface>& cryptIface) :
    Erase(devPathIn)
{

    /* get cryptHandle */
    CryptHandle cryptHandle(std::string(devPath).c_str());
    if (cryptHandle.get() == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }
    /* cryptLoad */
    if (cryptIface.get()->cryptLoad(cryptHandle.get(), CRYPT_LUKS2, nullptr) !=
        0)
    {
        lg2::error("Failed to load the key slots for destruction",
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }

    /* find key slots */
    int nKeySlots = cryptIface.get()->cryptKeySlotMax(CRYPT_LUKS2);
    if (nKeySlots <= 0)
    {
        lg2::error("Failed to find the max keyslots", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.EraseFailure"));
        throw ResourceNotFound();
    }

    /* destory working keyslots */
    bool keySlotIssue = false;
    for (int i = 0; i < nKeySlots; i++)
    {
        crypt_keyslot_info ki =
            cryptIface.get()->cryptKeySlotStatus(cryptHandle.get(), i);

        if (ki == CRYPT_SLOT_ACTIVE || ki == CRYPT_SLOT_ACTIVE_LAST)
        {
            if (cryptIface.get()->cryptKeyslotDestroy(cryptHandle.get(), i) !=
                0)
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
