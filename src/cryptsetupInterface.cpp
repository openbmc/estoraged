#include "cryptsetupInterface.hpp"

#include <libcryptsetup.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>
#include <string_view>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;

/** @class Cryptsetup
 *  @brief Implements CryptsetupInterface.
 */
int Cryptsetup::cryptFormat(struct crypt_device* cd, const char* type,
                            const char* cipher, const char* cipherMode,
                            const char* uuid, const char* volumeKey,
                            size_t volumeKeySize, void* params)
{
    return crypt_format(cd, type, cipher, cipherMode, uuid, volumeKey,
                        volumeKeySize, params);
}

int Cryptsetup::cryptKeyslotAddByVolumeKey(struct crypt_device* cd, int keyslot,
                                           const char* volumeKey,
                                           size_t volumeKeySize,
                                           const char* passphrase,
                                           size_t passphraseSize)
{
    return crypt_keyslot_add_by_volume_key(
        cd, keyslot, volumeKey, volumeKeySize, passphrase, passphraseSize);
}

int Cryptsetup::cryptLoad(struct crypt_device* cd, const char* requestedType,
                          void* params)
{
    return crypt_load(cd, requestedType, params);
}

int Cryptsetup::cryptActivateByPassphrase(struct crypt_device* cd,
                                          const char* name, int keyslot,
                                          const char* passphrase,
                                          size_t passphraseSize, uint32_t flags)
{
    return crypt_activate_by_passphrase(cd, name, keyslot, passphrase,
                                        passphraseSize, flags);
}

int Cryptsetup::cryptDeactivate(struct crypt_device* cd, const char* name)
{
    return crypt_deactivate(cd, name);
}

int Cryptsetup::cryptKeyslotDestroy(struct crypt_device* cd, const int keyslot)
{
    return crypt_keyslot_destroy(cd, keyslot);
}

int Cryptsetup::cryptKeySlotMax(const char* type)
{
    return crypt_keyslot_max(type);
}

crypt_keyslot_info Cryptsetup::cryptKeySlotStatus(struct crypt_device* cd,
                                                  int keyslot)
{
    return crypt_keyslot_status(cd, keyslot);
}

/** @class CryptHandle
 *  @brief This manages a crypt_device struct and automatically frees it when
 *  this handle exits the current scope.
 */
/** @brief Get a pointer to the crypt_device struct. */
struct crypt_device* CryptHandle::get()
{
    if (*handle == nullptr)
    {
        lg2::error("Failed to get crypt device handle", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.HandleGetFail"));
        throw ResourceNotFound();
    }

    return *handle;
}

/** @brief Allocate and initialize the crypt_device struct
 *
 *  @param[in] device - path to device file
 */
struct crypt_device* CryptHandle::init(const std::string_view& device)
{
    struct crypt_device* cryptDev = nullptr;
    int retval = crypt_init(&cryptDev, device.data());
    if (retval < 0)
    {
        lg2::error("Failed to crypt_init", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.InitFail"));
        throw ResourceNotFound();
    }

    return cryptDev;
}

/** @brief Free the crypt_device struct
 *
 *  @param[in] cd - pointer to crypt_device*, to be freed
 */
void CryptHandle::cryptFree(struct crypt_device*&& cd)
{
    crypt_free(cd);
}
} // namespace estoraged
