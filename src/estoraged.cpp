
#include "estoraged.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>
#include <stdlib.h>

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

/*
 * Format LUKS encrypted device.
 */
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
                         512 / 8, NULL);
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

/*
 * Unlock device.
 */
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
    /*
     * Run the command to create the filesystem:
     * mkfs.ext4 /dev/mapper/<name>
     */
    int retval = runCommand("mkfs.ext4 /dev/mapper/" + containerName);
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
    int retval = runCommand("mount -t ext4 /dev/mapper/" + containerName + " " +
                            mountPoint);
    if (retval)
    {
        lg2::error("Failed to mount filesystem: {RETVAL}", "RETVAL", retval,
                   "REDFISH_MESSAGE_ID",
                   std::string("OpenBMC.0.1.MountFilesystemFail"));
        bool unmountSuccess =
            removeDirectory(std::filesystem::path(mountPoint));
        if (!unmountSuccess)
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
    int retval = runCommand("umount " + mountPoint);
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
                           const char* cipher, const char* cipher_mode,
                           const char* uuid, const char* volume_key,
                           size_t volume_key_size, void* params)
{
    return crypt_format(cd, type, cipher, cipher_mode, uuid, volume_key,
                        volume_key_size, params);
}

int eStoraged::cryptKeyslotAddByVolumeKey(struct crypt_device* cd, int keyslot,
                                          const char* volume_key,
                                          size_t volume_key_size,
                                          const char* passphrase,
                                          size_t passphrase_size)
{
    return crypt_keyslot_add_by_volume_key(
        cd, keyslot, volume_key, volume_key_size, passphrase, passphrase_size);
}

int eStoraged::cryptActivateByPassphrase(struct crypt_device* cd,
                                         const char* name, int keyslot,
                                         const char* passphrase,
                                         size_t passphrase_size, uint32_t flags)
{
    return crypt_activate_by_passphrase(cd, name, keyslot, passphrase,
                                        passphrase_size, flags);
}

int eStoraged::cryptLoad(struct crypt_device* cd, const char* requested_type,
                         void* params)
{
    return crypt_load(cd, requested_type, params);
}

int eStoraged::runCommand(const std::string& command)
{
    return system(command.c_str());
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
