#include "cryptoErase.hpp"

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

CryptoErase::CryptoErase(
    std::string_view devPath, int keyslot,
    std::unique_ptr<estoraged::CryptsetupInterface>& cryptIface) :
    Erase("")
{

    CryptHandle cryptHandle(std::string(devPath).c_str());
    if (cryptHandle.get() == nullptr)
    {
        lg2::error("Failed to initialize crypt device", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatFail"));
        throw ResourceNotFound();
    }
    if (cryptIface.get()->cryptKeyslotDestroy(cryptHandle.get(), keyslot) != 0)
    {
        lg2::error("Estoraged erase failed to destroy keyslot",
                   "REDFISH_MESSAGE_ID",
                   std::string("eStorageD.1.0.EraseFailure"));

        throw InternalFailure();
    }

    /*

        // if multiple EstorageDs are running this will need to be updated
        int rc = crypt_keyslot_destroy(cd, keyslot);
        if (rc != 0)
        {
            lg2::error("Estoraged erase failed to destory keyslot",
                       "REDFISH_MESSAGE_ID",
                       std::string("eStorageD.1.0.EraseFailure"));

            throw InternalFailure();
        }
        // rc = action_luksErase();
    */
}

} // namespace estoraged
