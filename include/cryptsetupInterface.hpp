#pragma once

#include <libcryptsetup.h>

#include <phosphor-logging/lg2.hpp>
#include <stdplus/handle/managed.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <string>
#include <string_view>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;

/** @class CryptsetupInterface
 *  @brief Interface to the cryptsetup functions used to manage a LUKS device.
 *  @details This class is used to mock out the cryptsetup functions.
 */
class CryptsetupInterface
{
  public:
    virtual ~CryptsetupInterface() = default;
    CryptsetupInterface() = default;
    CryptsetupInterface(const CryptsetupInterface&) = delete;
    CryptsetupInterface& operator=(const CryptsetupInterface&) = delete;

    CryptsetupInterface(CryptsetupInterface&&) = delete;
    CryptsetupInterface& operator=(CryptsetupInterface&&) = delete;

    /** @brief Wrapper around crypt_format.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle.
     *  @param[in] type - type of device (optional params struct must be of
     *    this type).
     *  @param[in] cipher - (e.g. "aes").
     *  @params[in cipher_mode - including IV specification (e.g. "xts-plain").
     *  @params[in] uuid - requested UUID or NULL if it should be generated.
     *  @params[in] volume_key - pre-generated volume key or NULL if it should
     *    be generated (only for LUKS).
     *  @params[in] volume_key_size - size of volume key in bytes.
     *  @params[in] params - crypt type specific parameters.
     *
     *  @returns 0 on success or negative errno value otherwise.
     */
    virtual int cryptFormat(struct crypt_device* cd, const char* type,
                            const char* cipher, const char* cipherMode,
                            const char* uuid, const char* volumeKey,
                            size_t volumeKeySize, void* params) = 0;

    /** @brief Wrapper around crypt_keyslot_change_by_passphrase.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle.
     *  @param[in] keyslotOld - old keyslot or CRYPT_ANY_SLOT.
     *  @param[in] keyslotNew - new keyslot or CRYPT_ANY_SLOT.
     *  @param[in] passphrase - passphrase for new keyslot.
     *  @param[in] passphraseSize - size of passphrase.
     *  @param[in] newPassphrase - new passphrase for the specified keyslot
     *  @param[in] newPassphraseSize - size of newPassphrase (in bytes).
     *
     *  @returns allocated key slot number or negative errno otherwise.
     */
    virtual int cryptKeyslotChangeByPassphrase(struct crypt_device* cd,
                                               int keyslotOld, int keyslotNew,
                                               const char* passphrase,
                                               size_t passphraseSize,
                                               const char* newPassphrase,
                                               size_t newPassphraseSize) = 0;

    /** @brief Wrapper around crypt_keyslot_add_by_volume_key.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle.
     *  @param[in] keyslot - requested keyslot or CRYPT_ANY_SLOT.
     *  @param[in] volume_key - provided volume key or NULL if used after
     *    crypt_format.
     *  @param[in] volume_key_size - size of volume_key.
     *  @param[in] passphrase - passphrase for new keyslot.
     *  @param[in] passphrase_size - size of passphrase.
     *
     *  @returns allocated key slot number or negative errno otherwise.
     */
    virtual int cryptKeyslotAddByVolumeKey(struct crypt_device* cd, int keyslot,
                                           const char* volumeKey,
                                           size_t volumeKeySize,
                                           const char* passphrase,
                                           size_t passphraseSize) = 0;

    /** @brief Wrapper around crypt_load.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle.
     *  @param[in] requested_type - crypt-type or NULL for all known.
     *  @param[in] params - crypt type specific parameters (see crypt-type).
     *
     *  @returns 0 on success or negative errno value otherwise.
     */
    virtual int cryptLoad(struct crypt_device* cd, const char* requestedType,
                          void* params) = 0;

    /** @brief Wrapper around crypt_activate_by_passphrase.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle.
     *  @param[in] name - name of device to create, if NULL only check
     *    passphrase.
     *  @param[in] keyslot - requested keyslot to check or CRYPT_ANY_SLOT.
     *  @param[in] passphrase - passphrase used to unlock volume key.
     *  @param[in] passphrase_size - size of passphrase.
     *  @param[in] flags - activation flags.
     *
     *  @returns unlocked key slot number or negative errno otherwise.
     */
    virtual int cryptActivateByPassphrase(struct crypt_device* cd,
                                          const char* name, int keyslot,
                                          const char* passphrase,
                                          size_t passphraseSize,
                                          uint32_t flags) = 0;

    /** @brief Wrapper around crypt_deactivate.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle, can be NULL.
     *  @param[in] name - name of device to deactivate.
     *
     *  @returns 0 on success or negative errno value otherwise.
     */
    virtual int cryptDeactivate(struct crypt_device* cd, const char* name) = 0;

    /** @brief Wrapper around crypt_keyslot_destory.
     *  @details Used for mocking purposes.
     *
     *  @param[in] cd - crypt device handle, can not be NULL.
     *  @param[in] keyslot requested key slot to destroy
     *
     *  @returns 0 on success or negative errno value otherwise.
     */
    virtual int cryptKeyslotDestroy(struct crypt_device* cd, int keyslot) = 0;

    /** @breif Wapper around crypt_keyslot_max
     *  @details Used for mocking purposes.
     *
     * @param type crypt device type
     *
     * @return slot count or negative errno otherwise if device
     * does not support keyslots.
     */
    virtual int cryptKeySlotMax(const char* type) = 0;

    /** @breif Wapper around crypt_keyslot_status
     *  @details Used for mocking purposes.
     *  Get information about particular key slot.
     *
     * @param cd crypt device handle
     * @param keyslot requested keyslot to check or CRYPT_ANY_SLOT
     *
     * @return value defined by crypt_keyslot_info
     *
     */
    virtual crypt_keyslot_info cryptKeySlotStatus(struct crypt_device* cd,
                                                  int keyslot) = 0;
};

/** @class Cryptsetup
 *  @brief Implements CryptsetupInterface.
 */
class Cryptsetup : public CryptsetupInterface
{
  public:
    ~Cryptsetup() override = default;

    Cryptsetup() = default;
    Cryptsetup(const Cryptsetup&) = delete;
    Cryptsetup& operator=(const Cryptsetup&) = delete;

    Cryptsetup(Cryptsetup&&) = delete;
    Cryptsetup& operator=(Cryptsetup&&) = delete;
    int cryptFormat(struct crypt_device* cd, const char* type,
                    const char* cipher, const char* cipherMode,
                    const char* uuid, const char* volumeKey,
                    size_t volumeKeySize, void* params) override
    {
        return crypt_format(cd, type, cipher, cipherMode, uuid, volumeKey,
                            volumeKeySize, params);
    }

    int cryptKeyslotChangeByPassphrase(struct crypt_device* cd, int keyslotOld,
                                       int keyslotNew, const char* passphrase,
                                       size_t passphraseSize,
                                       const char* newPassphrase,
                                       size_t newPassphraseSize) override
    {
        return crypt_keyslot_change_by_passphrase(
            cd, keyslotOld, keyslotNew, passphrase, passphraseSize,
            newPassphrase, newPassphraseSize);
    }

    int cryptKeyslotAddByVolumeKey(struct crypt_device* cd, int keyslot,
                                   const char* volumeKey, size_t volumeKeySize,
                                   const char* passphrase,
                                   size_t passphraseSize) override
    {
        return crypt_keyslot_add_by_volume_key(
            cd, keyslot, volumeKey, volumeKeySize, passphrase, passphraseSize);
    }

    int cryptLoad(struct crypt_device* cd, const char* requestedType,
                  void* params) override
    {
        return crypt_load(cd, requestedType, params);
    }

    int cryptActivateByPassphrase(struct crypt_device* cd, const char* name,
                                  int keyslot, const char* passphrase,
                                  size_t passphraseSize,
                                  uint32_t flags) override
    {
        return crypt_activate_by_passphrase(cd, name, keyslot, passphrase,
                                            passphraseSize, flags);
    }

    int cryptDeactivate(struct crypt_device* cd, const char* name) override
    {
        return crypt_deactivate(cd, name);
    }

    int cryptKeyslotDestroy(struct crypt_device* cd, const int keyslot) override
    {
        return crypt_keyslot_destroy(cd, keyslot);
    }

    int cryptKeySlotMax(const char* type) override
    {
        return crypt_keyslot_max(type);
    }

    crypt_keyslot_info cryptKeySlotStatus(struct crypt_device* cd,
                                          int keyslot) override
    {
        return crypt_keyslot_status(cd, keyslot);
    }
};

/** @class CryptHandle
 *  @brief This manages a crypt_device struct and automatically frees it when
 *  this handle exits the current scope.
 */
class CryptHandle
{
  public:
    /** @brief Constructor for CryptHandle
     *
     *  @param[in] device - path to device file
     */
    explicit CryptHandle(const std::string_view& device) : handle(init(device))
    {}

    /** @brief Get a pointer to the crypt_device struct. */
    struct crypt_device* get()
    {
        if (*handle == nullptr)
        {
            lg2::error("Failed to get crypt device handle",
                       "REDFISH_MESSAGE_ID",
                       std::string("OpenBMC.0.1.HandleGetFail"));
            throw ResourceNotFound();
        }

        return *handle;
    }

  private:
    /** @brief Allocate and initialize the crypt_device struct
     *
     *  @param[in] device - path to device file
     */
    struct crypt_device* init(const std::string_view& device)
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
    static void cryptFree(struct crypt_device*&& cd)
    {
        crypt_free(cd);
    }

    /** @brief Managed handle to crypt_device struct */
    stdplus::Managed<struct crypt_device*>::Handle<cryptFree> handle;
};

} // namespace estoraged
