
#include "estoraged.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <sys/mount.h>

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/eStoraged/error.hpp>

#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace estoraged
{

using sdbusplus::xyz::openbmc_project::eStoraged::Error::EncryptionError;
using sdbusplus::xyz::openbmc_project::eStoraged::Error::FilesystemError;

void eStoraged::format(std::vector<uint8_t> password)
{
    std::string msg = "OpenBMC.0.1.DriveFormat";
    lg2::info("Starting format", "REDFISH_MESSAGE_ID", msg);

    formatLuksDev(password);
    activateLuksDev(password);

    createFilesystem();
    mountFilesystem();
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    std::string msg = "OpenBMC.0.1.DriveErase";
    lg2::info("Starting erase", "REDFISH_MESSAGE_ID", msg);
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::string msg = "OpenBMC.0.1.DriveLock";
    lg2::info("Starting lock", "REDFISH_MESSAGE_ID", msg);

    unmountFilesystem();
    deactivateLuksDev();
}

void eStoraged::unlock(std::vector<uint8_t> password)
{
    std::string msg = "OpenBMC.0.1.DriveUnlock";
    lg2::info("Starting unlock", "REDFISH_MESSAGE_ID", msg);

    activateLuksDev(password);
    mountFilesystem();
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    std::string msg = "OpenBMC.0.1.DrivePasswordChanged";
    lg2::info("Starting change password", "REDFISH_MESSAGE_ID", msg);
}

bool eStoraged::isLocked() const
{
    return locked();
}

std::string_view eStoraged::getMountPoint() const
{
    return mountPoint;
}

void eStoraged::formatLuksDev(std::vector<uint8_t> password)
{
    lg2::info("Formatting device {DEV}", "DEV", devPath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.FormatLuksDev"));

    struct crypt_device* cryptDev;
    int retval = crypt_init(&cryptDev, devPath.c_str());
    if (retval < 0)
    {
        lg2::error("Failed to initialize crypt device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw EncryptionError();
    }

    /* Generate the volume key. */
    const std::size_t keySize = 64;
    std::vector<uint8_t> volumeKey(keySize);
    if (RAND_bytes(volumeKey.data(), keySize) != 1)
    {
        lg2::error("Failed to create volume key", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        throw EncryptionError();
    }
    /* Format the LUKS encrypted device. */
    retval = cryptFormat(cryptDev, CRYPT_LUKS2, "aes", "xts-plain64", NULL,
                         reinterpret_cast<const char*>(volumeKey.data()),
                         volumeKey.size(), NULL);
    if (retval < 0)
    {
        lg2::error("Failed to format encrypted device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    /* Device is now encrypted. */
    locked(true);

    /* Set the password. */
    retval = cryptKeyslotAddByVolumeKey(
        cryptDev, CRYPT_ANY_SLOT, NULL, 0,
        reinterpret_cast<const char*>(password.data()), password.size());

    if (retval < 0)
    {
        lg2::error("Failed to set encryption password", "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.FormatLuksDevFail"));
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    lg2::info("Encrypted device {DEV} successfully formatted", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.FormatLuksDevSuccess"));

    crypt_free(cryptDev);
}

void eStoraged::activateLuksDev(std::vector<uint8_t> password)
{
    lg2::info("Activating LUKS dev {DEV}", "DEV", devPath, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDev"));

    struct crypt_device* cryptDev;
    int retval = crypt_init(&cryptDev, devPath.c_str());
    if (retval < 0)
    {
        lg2::error("Failed to initialize crypt device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        throw EncryptionError();
    }

    retval = cryptLoad(cryptDev, CRYPT_LUKS2, NULL);
    if (retval < 0)
    {
        lg2::error("Failed to load LUKS header: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    retval = cryptActivateByPassphrase(
        cryptDev, containerName.c_str(), CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(password.data()), password.size(), 0);

    if (retval < 0)
    {
        lg2::error("Failed to activate LUKS dev: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.ActivateLuksDevFail"));
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    /* Device is now unlocked. */
    locked(false);

    lg2::info("Successfully activated LUKS dev {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.ActivateLuksDevSuccess"));
    crypt_free(cryptDev);
}

void eStoraged::createFilesystem()
{
    /* Run the command to create the filesystem. */
    int retval = runMkfs();
    if (retval)
    {
        lg2::error("Failed to create filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.CreateFilesystemFail"));
        throw FilesystemError();
    }
    lg2::info("Successfully created filesystem for /dev/mapper/{CONTAINER}",
              "CONTAINER", containerName, "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.CreateFilesystemSuccess"));
}

void eStoraged::mountFilesystem()
{
    /* Create directory for the filesystem. */
    bool success = createDirectory(std::filesystem::path(mountPoint));
    if (!success)
    {
        lg2::error("Failed to create mount point: {DIR}", "DIR", mountPoint,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.MountFilesystemFail"));
        throw FilesystemError();
    }

    /* Run the command to mount the filesystem. */
    std::string luksContainer("/dev/mapper/" + containerName);
    int retval =
        doMount(luksContainer.c_str(), mountPoint.c_str(), "ext4", 0, NULL);
    if (retval)
    {
        lg2::error("Failed to mount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.MountFilesystemFail"));
        bool removeSuccess = removeDirectory(std::filesystem::path(mountPoint));
        if (!removeSuccess)
        {
            lg2::error("Failed to remove mount point: {DIR}", "DIR", mountPoint,
                       "REDFISH_MESSAGE_ID",
                       std::string("OpenBMC.0.1.MountFilesystemFail"));
        }
        throw FilesystemError();
    }

    lg2::info("Successfully mounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

void eStoraged::unmountFilesystem()
{
    int retval = doUnmount(mountPoint.c_str());
    if (retval)
    {
        lg2::error("Failed to unmount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw FilesystemError();
    }

    /* Remove the mount point. */
    bool success = removeDirectory(std::filesystem::path(mountPoint));
    if (!success)
    {
        lg2::error("Failed to remove mount point {DIR}", "DIR", mountPoint,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.UnmountFilesystemFail"));
        throw FilesystemError();
    }

    lg2::info("Successfully unmounted filesystem at {DIR}", "DIR", mountPoint,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.MountFilesystemSuccess"));
}

/*
 * Lock the LUKS device.
 */
void eStoraged::deactivateLuksDev()
{
    lg2::info("Deactivating LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDev"));

    struct crypt_device* cryptDev = NULL;
    int retval = cryptInitByName(&cryptDev, containerName.c_str());
    if (retval < 0)
    {
        lg2::error("Failed to initialize crypt device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DeactivateLuksDevFail"));
        throw EncryptionError();
    }

    retval = cryptDeactivate(cryptDev, containerName.c_str());
    if (retval < 0)
    {
        lg2::error("Failed to deactivate crypt device: {RETVAL}", "RETVAL",
                   retval, "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.DeactivateLuksDevFail"));
        throw EncryptionError();
    }
    crypt_free(cryptDev);

    /* Device is now locked. */
    locked(true);

    lg2::info("Successfully deactivated LUKS device {DEV}", "DEV", devPath,
              "REDFISH_MESSAGE_ID",
              std::string("OpenBMC.0.1.DeactivateLuksDevSuccess"));
}

int eStoraged::cryptFormat(struct crypt_device* cd, const char* type,
                           const char* cipher, const char* cipherMode,
                           const char* uuid, const char* volumeKey,
                           size_t volumeKeySize, void* params)
{
    return crypt_format(cd, type, cipher, cipherMode, uuid, volumeKey,
                        volumeKeySize, params);
}

int eStoraged::cryptKeyslotAddByVolumeKey(struct crypt_device* cd, int keyslot,
                                          const char* volumeKey,
                                          size_t volumeKeySize,
                                          const char* passphrase,
                                          size_t passphraseSize)
{
    return crypt_keyslot_add_by_volume_key(
        cd, keyslot, volumeKey, volumeKeySize, passphrase, passphraseSize);
}

int eStoraged::cryptActivateByPassphrase(struct crypt_device* cd,
                                         const char* name, int keyslot,
                                         const char* passphrase,
                                         size_t passphraseSize, uint32_t flags)
{
    return crypt_activate_by_passphrase(cd, name, keyslot, passphrase,
                                        passphraseSize, flags);
}

int eStoraged::cryptLoad(struct crypt_device* cd, const char* requestedType,
                         void* params)
{
    return crypt_load(cd, requestedType, params);
}

int eStoraged::runMkfs()
{
    std::string mkfsCommand("mkfs.ext4 /dev/mapper/" + containerName);
    return system(mkfsCommand.c_str());
}

int eStoraged::doMount(const char* source, const char* target,
                       const char* filesystemtype, unsigned long mountflags,
                       const void* data)
{
    return mount(source, target, filesystemtype, mountflags, data);
}

int eStoraged::doUnmount(const char* target)
{
    return umount(target);
}

int eStoraged::cryptInitByName(struct crypt_device** cd, const char* name)
{
    return crypt_init_by_name(cd, name);
}

int eStoraged::cryptDeactivate(struct crypt_device* cd, const char* name)
{
    return crypt_deactivate(cd, name);
}

bool eStoraged::createDirectory(const std::filesystem::path& p)
{
    return std::filesystem::create_directory(p);
}

bool eStoraged::removeDirectory(const std::filesystem::path& p)
{
    return std::filesystem::remove(p);
}

} // namespace estoraged
