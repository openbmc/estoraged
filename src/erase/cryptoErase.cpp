#include "cryptoErase.hpp"

#include "erase.hpp"

#include <libcryptsetup.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

CryptoErase::CryptoErase(struct crypt_device* cd, int keyslot) : Erase("")
{
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
}
