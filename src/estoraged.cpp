
#include "estoraged.hpp"

#include "logging.hpp"

#include <libcryptsetup.h>
#include <openssl/rand.h>
#include <stdlib.h>

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
    std::cerr << "Formatting encrypted eMMC" << std::endl;
    log("MESSAGE=starting Format"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveFormat", NULL);

    formatLuksDev(password);
    activateLuksDev(password);

    /* Set Locked property to false. */
    locked(false);

    createFilesystem();
    mountFilesystem();
}

void eStoraged::erase(std::vector<uint8_t>, EraseMethod)
{
    std::cerr << "Erasing encrypted eMMC" << std::endl;
    log("MESSAGE=starting Erase"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveErase", NULL);
}

void eStoraged::lock(std::vector<uint8_t>)
{
    std::cerr << "Locking encrypted eMMC" << std::endl;
    log("MESSAGE=starting Lock"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveLock", NULL);

    unmountFilesystem();
    deactivateLuksDev();
    locked(true);
}

void eStoraged::unlock(std::vector<uint8_t> password)
{
    std::cerr << "Unlocking encrypted eMMC" << std::endl;
    log("MESSAGE=starting unlock"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DriveUnlock", NULL);

    activateLuksDev(password);
    locked(false);
    mountFilesystem();
}

void eStoraged::changePassword(std::vector<uint8_t>, std::vector<uint8_t>)
{
    std::cerr << "Changing password for encrypted eMMC" << std::endl;
    log("MESSAGE=starting changing password"
        "REDFISH_MESSAGE_ID=%s",
        "OpenBMC.0.1.DrivePasswordChanged", NULL);
}

bool eStoraged::is_locked(void) const
{
    return locked();
}

std::string_view eStoraged::getMountPoint(void) const
{
    return mountPoint;
}

/*
 * Format LUKS encrypted device.
 */
void eStoraged::formatLuksDev(std::vector<uint8_t> password)
{
    log("MESSAGE=formatting device %s", devPath.c_str(), NULL);

    struct crypt_device* cryptDev;
    int retval = crypt_init(&cryptDev, devPath.c_str());
    if (retval < 0)
    {
        log("MESSAGE=failed to initialize crypt device: %d", retval, NULL);
        throw EncryptionError();
    }

    /* Generate the volume key. */
    const std::size_t keySize = 64;
    std::vector<uint8_t> volumeKey(keySize);
    if (RAND_bytes(volumeKey.data(), keySize) != 1)
    {
        log("MESSAGE=failed to create volume key", NULL);
        throw EncryptionError();
    }
    /* Format the LUKS encrypted device. */
    retval = crypt_format(cryptDev, CRYPT_LUKS2, "aes", "xts-plain64", NULL,
                          reinterpret_cast<const char*>(volumeKey.data()),
                          512 / 8, NULL);

    if (retval)
    {
        log("MESSAGE=failed to format encrypted device", NULL);
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    /* Set the password. */
    retval = crypt_keyslot_add_by_volume_key(
        cryptDev, CRYPT_ANY_SLOT, NULL, 0,
        reinterpret_cast<const char*>(password.data()), password.size());

    if (retval < 0)
    {
        log("MESSAGE=failed to set encryption password", NULL);
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    log("MESSAGE=encrypted device %s successfully formatted", devPath.c_str(),
        NULL);

    crypt_free(cryptDev);
}

/*
 * Unlock device.
 */
void eStoraged::activateLuksDev(std::vector<uint8_t> password)
{
    log("MESSAGE=activating LUKS dev %s", devPath.c_str(), NULL);

    struct crypt_device* cryptDev;
    int retval = crypt_init(&cryptDev, devPath.c_str());
    if (retval < 0)
    {
        log("MESSAGE=failed to initialize crypt device: %d", retval, NULL);
        throw EncryptionError();
    }

    retval = crypt_load(cryptDev, CRYPT_LUKS2, NULL);
    if (retval < 0)
    {
        log("MESSAGE=failed to load LUKS header: %d", retval, NULL);
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    retval = crypt_activate_by_passphrase(
        cryptDev, containerName.c_str(), CRYPT_ANY_SLOT,
        reinterpret_cast<const char*>(password.data()), password.size(), 0);

    if (retval < 0)
    {
        log("MESSAGE=failed to activate device", NULL);
        crypt_free(cryptDev);
        throw EncryptionError();
    }

    log("MESSAGE=Successfully activated LUKS dev %s", devPath.c_str(), NULL);
    crypt_free(cryptDev);
}

void eStoraged::createFilesystem()
{
    /*
     * Build the command to create the filesystem:
     * mkfs.ext4 /dev/mapper/<name>
     */
    std::string mkfsCommand = "mkfs.ext4 /dev/mapper/" + containerName;
    int retval = system(mkfsCommand.c_str());
    if (retval)
    {
        log("MESSAGE=failed to create filesystem", NULL);
        throw FilesystemError();
    }
    log("MESSAGE=Successfully created filesystem for /dev/mapper/%s",
        containerName.c_str(), NULL);
}

void eStoraged::mountFilesystem()
{
    /* Create directory for the filesystem. */
    bool success =
        std::filesystem::create_directory(std::filesystem::path(mountPoint));
    if (!success)
    {
        log("MESSAGE=failed to create the mount point", NULL);
        throw FilesystemError();
    }

    /* Build the command to mount the filesystem. */
    std::string mountCommand =
        "mount -t ext4 /dev/mapper/" + containerName + " " + mountPoint;

    /* Mount the filesystem. */
    int retval = system(mountCommand.c_str());
    if (retval)
    {
        log("MESSAGE=failed to mount the filesystem", NULL);
        throw FilesystemError();
    }

    log("MESSAGE=Successfully mounted filesystem at %s", mountPoint.c_str(),
        NULL);
}

void eStoraged::unmountFilesystem()
{
    std::string umountCommand = "umount " + mountPoint;
    int retval = system(umountCommand.c_str());
    if (retval)
    {
        log("MESSAGE=failed to unmount: %d", retval, NULL);
        throw FilesystemError();
    }

    /* Remove the mount point. */
    bool success = std::filesystem::remove(std::filesystem::path(mountPoint));
    if (!success)
    {
        log("MESSAGE=failed to remove the mount point", NULL);
        throw FilesystemError();
    }

    log("MESSAGE=Successfully unmounted filesystem at %s", mountPoint.c_str(),
        NULL);
}

/*
 * Lock the LUKS device.
 */
void eStoraged::deactivateLuksDev()
{
    log("MESSAGE=Deactivating LUKS device %s", devPath.c_str(), NULL);

    struct crypt_device* cryptDev;
    int retval = crypt_init_by_name(&cryptDev, containerName.c_str());
    if (retval < 0)
    {
        log("MESSAGE=failed to initialize crypt device: %d", retval, NULL);
        throw EncryptionError();
    }

    retval = crypt_deactivate(cryptDev, containerName.c_str());
    if (retval < 0)
    {
        log("MESSAGE=failed to deactivate crypt device: %d", retval, NULL);
        throw EncryptionError();
    }
    crypt_free(cryptDev);

    log("MESSAGE=Successfully deactivating LUKS device %s", devPath.c_str(),
        NULL);
}

} // namespace estoraged
